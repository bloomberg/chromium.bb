// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/allocation_tracker.h"

#include "base/callback.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/profiling_globals.h"

namespace profiling {

AllocationTracker::Alloc::Alloc(size_t sz, BacktraceStorage::Key key)
    : size(sz), backtrace_key(key) {}

AllocationTracker::AllocationTracker(CompleteCallback complete_cb)
    : complete_callback_(std::move(complete_cb)),
      backtrace_storage_(ProfilingGlobals::Get()->GetBacktraceStorage()) {}

AllocationTracker::~AllocationTracker() {
  std::vector<BacktraceStorage::Key> to_free;
  to_free.reserve(live_allocs_.size());
  for (const auto& cur : live_allocs_)
    to_free.push_back(cur.second.backtrace_key);
  backtrace_storage_->Free(to_free);
}

void AllocationTracker::OnHeader(const StreamHeader& header) {}

void AllocationTracker::OnAlloc(const AllocPacket& alloc_packet,
                                std::vector<Address>&& bt) {
  BacktraceStorage::Key backtrace_key =
      backtrace_storage_->Insert(std::move(bt));
  live_allocs_.emplace(Address(alloc_packet.address),
                       Alloc(alloc_packet.size, backtrace_key));
}

void AllocationTracker::OnFree(const FreePacket& free_packet) {
  auto found = live_allocs_.find(Address(free_packet.address));
  if (found != live_allocs_.end()) {
    backtrace_storage_->Free(found->second.backtrace_key);
    live_allocs_.erase(found);
  }
}

void AllocationTracker::OnComplete() {
  std::move(complete_callback_).Run();
  // Danger: object may be deleted now.
}

}  // namespace profiling
