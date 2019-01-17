/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include <thread>
#include <vector>

#include <folly/MPMCQueue.h>
#include <folly/SharedMutex.h>

#include "logdevice/common/configuration/nodes/NodesConfigurationStore.h"

namespace facebook { namespace logdevice { namespace configuration {
namespace nodes {

// Note: it's not safe to have multiple FileBasedNodesConfigurationStore objects
// created from the `root_path' accessing configs with the same `key'
// concurrently. For the best practice, use only one
// FileBasedNodesConfigurationStore instance for one `root_path'.
class FileBasedNodesConfigurationStore : public NodesConfigurationStore {
 public:
  explicit FileBasedNodesConfigurationStore(std::string root_path,
                                            extract_version_fn f);

  ~FileBasedNodesConfigurationStore();

  void getConfig(std::string key, value_callback_t cb) const override;

  void updateConfig(std::string key,
                    std::string value,
                    folly::Optional<version_t> base_version,
                    write_callback_t cb = {}) override;

 private:
  const std::string root_path_;
  extract_version_fn extract_fn_;

  // task thread for async tasks
  std::atomic<bool> shutdown_{false};
  mutable folly::MPMCQueue<folly::Function<void()>> task_queue_;
  std::vector<std::thread> task_threads_;

  void threadMain();

  // must be executed on the TaskThread
  void getConfigImpl(std::string key, value_callback_t cb) const;
  void updateConfigImpl(std::string key,
                        std::string value,
                        folly::Optional<version_t> base_version,
                        write_callback_t cb);

  std::string getDataFilePath(const std::string& key) const {
    return root_path_ + "/" + key;
  }

  std::string getLockFilePath(const std::string& key) const {
    return root_path_ + "/" + key + ".lock__";
  }

  // TODO: flock() only works for inter-process coordination but within the
  // process two threads can acquire the same flock at the same time. currently
  // the store task thread pool is single threaded to prevent the problem. In
  // the future if we ever want the file store to be multi-threaded, we need to
  // add a per-key shared_locks for coordination.
  static constexpr size_t NUM_THREADS = 1;
  static constexpr size_t QUEUE_SIZE = 8;
  // value is capped at 10MB
  static constexpr size_t MAX_VALUE_SIZE_IN_BYTES = 10 * 1024 * 1024;
};

}}}} // namespace facebook::logdevice::configuration::nodes