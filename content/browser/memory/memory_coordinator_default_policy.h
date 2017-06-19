// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_DEFAULT_POLICY_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_DEFAULT_POLICY_H_

#include "content/browser/memory/memory_coordinator_impl.h"

namespace content {

// The default policy of MemoryCoordinator.
class MemoryCoordinatorDefaultPolicy : public MemoryCoordinatorImpl::Policy {
 public:
  explicit MemoryCoordinatorDefaultPolicy(MemoryCoordinatorImpl* coordinator);
  ~MemoryCoordinatorDefaultPolicy() override;

  void OnCriticalCondition() override;
  void OnConditionChanged(MemoryCondition prev, MemoryCondition next) override;
  void OnChildVisibilityChanged(int render_process_id,
                                bool is_visible) override;

 private:
  // Set the provided memory_state to all child processes.
  void SetMemoryStateToAllChildren(MemoryState memory_state);

  enum class PurgeTarget {
    BACKGROUNDED,
    ALL,
  };

  // Tries to find a candidate child process for purging memory and asks the
  // child to purge memory.
  bool TryToPurgeMemoryFromChildren(PurgeTarget target);

  // Not owned.
  MemoryCoordinatorImpl* coordinator_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_DEFAULT_POLICY_H_
