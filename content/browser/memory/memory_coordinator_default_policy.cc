// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_default_policy.h"

namespace content {

namespace {

const int kDefaultBackgroundChildPurgeCandidatePeriodSeconds = 30;

MemoryState CalculateMemoryStateForProcess(MemoryCondition condition) {
  if (condition == MemoryCondition::CRITICAL)
    return MemoryState::THROTTLED;
  return MemoryState::NORMAL;
}

}  // namespace

MemoryCoordinatorDefaultPolicy::MemoryCoordinatorDefaultPolicy(
    MemoryCoordinatorImpl* coordinator)
    : coordinator_(coordinator) {
  DCHECK(coordinator_);
}

MemoryCoordinatorDefaultPolicy::~MemoryCoordinatorDefaultPolicy() {}

void MemoryCoordinatorDefaultPolicy::OnCriticalCondition() {
  coordinator_->DiscardTab();

  // Prefer to purge memory from child processes than browser process because
  // we prioritize the brower process.
  if (TryToPurgeMemoryFromChildren(PurgeTarget::ALL))
    return;

  coordinator_->TryToPurgeMemoryFromBrowser();
}

void MemoryCoordinatorDefaultPolicy::OnConditionChanged(MemoryCondition prev,
                                                        MemoryCondition next) {
  DCHECK(prev != next);
  if (next == MemoryCondition::NORMAL) {
    // Set NORMAL state to all clients/processes.
    coordinator_->SetBrowserMemoryState(MemoryState::NORMAL);
    SetMemoryStateToAllChildren(MemoryState::NORMAL);
  } else if (next == MemoryCondition::CRITICAL) {
    // Set THROTTLED state to all clients/processes.
    coordinator_->SetBrowserMemoryState(MemoryState::THROTTLED);
    SetMemoryStateToAllChildren(MemoryState::THROTTLED);
  }
}

void MemoryCoordinatorDefaultPolicy::OnChildVisibilityChanged(
    int render_process_id,
    bool is_visible) {
  auto iter = coordinator_->children().find(render_process_id);
  if (iter == coordinator_->children().end())
    return;

  iter->second.is_visible = is_visible;
  if (!is_visible) {
    // A backgrounded process becomes a candidate for purging memory when
    // the process remains backgrounded for a certian period of time.
    iter->second.can_purge_after =
        coordinator_->NowTicks() +
        base::TimeDelta::FromSeconds(
            kDefaultBackgroundChildPurgeCandidatePeriodSeconds);
  }
  MemoryState new_state =
      CalculateMemoryStateForProcess(coordinator_->GetMemoryCondition());
  coordinator_->SetChildMemoryState(render_process_id, new_state);
}

void MemoryCoordinatorDefaultPolicy::SetMemoryStateToAllChildren(
    MemoryState state) {
  // It's OK to call SetChildMemoryState() unconditionally because it checks
  // whether this state transition is valid.
  for (auto& iter : coordinator_->children())
    coordinator_->SetChildMemoryState(iter.first, state);
}

bool MemoryCoordinatorDefaultPolicy::TryToPurgeMemoryFromChildren(
    PurgeTarget target) {
  // TODO(bashi): Better to sort child processes based on their priorities.
  for (auto& iter : coordinator_->children()) {
    if (iter.second.is_visible && target == PurgeTarget::BACKGROUNDED)
      continue;
    if (coordinator_->TryToPurgeMemoryFromChild(iter.first))
      return true;
  }
  return false;
}

}  // namespace content
