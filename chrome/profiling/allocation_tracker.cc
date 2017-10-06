// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/allocation_tracker.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/profiling/backtrace_storage.h"

namespace profiling {

AllocationTracker::AllocationTracker(CompleteCallback complete_cb,
                                     BacktraceStorage* backtrace_storage)
    : complete_callback_(std::move(complete_cb)),
      backtrace_storage_(backtrace_storage) {}

AllocationTracker::~AllocationTracker() {
  std::vector<const Backtrace*> to_free;
  to_free.reserve(live_allocs_.size());
  for (const auto& cur : live_allocs_)
    to_free.push_back(cur.backtrace());
  backtrace_storage_->Free(to_free);
}

void AllocationTracker::OnHeader(const StreamHeader& header) {}

void AllocationTracker::OnAlloc(const AllocPacket& alloc_packet,
                                std::vector<Address>&& bt,
                                std::string&& context) {
  // Compute the context ID for this allocation, 0 means no context.
  int context_id = 0;
  if (!context.empty()) {
    auto inserted_record = context_.emplace(
        std::piecewise_construct, std::forward_as_tuple(std::move(context)),
        std::forward_as_tuple(next_context_id_));
    context_id = inserted_record.first->second;
    if (inserted_record.second)
      next_context_id_++;
  }

  const Backtrace* backtrace = backtrace_storage_->Insert(std::move(bt));
  live_allocs_.emplace(alloc_packet.allocator, Address(alloc_packet.address),
                       alloc_packet.size, backtrace, context_id);
}

void AllocationTracker::OnFree(const FreePacket& free_packet) {
  AllocationEvent find_me(Address(free_packet.address));
  auto found = live_allocs_.find(find_me);
  if (found != live_allocs_.end()) {
    backtrace_storage_->Free(found->backtrace());
    live_allocs_.erase(found);
  }
}

void AllocationTracker::OnComplete() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(complete_callback_));
}

AllocationCountMap AllocationTracker::GetCounts() const {
  return AllocationEventSetToCountMap(live_allocs_);
}

}  // namespace profiling
