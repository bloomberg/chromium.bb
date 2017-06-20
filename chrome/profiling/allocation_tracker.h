// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_ALLOCATION_TRACKER_H_
#define CHROME_PROFILING_ALLOCATION_TRACKER_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/memlog_receiver.h"

namespace profiling {

// Tracks live allocations in one process. This is an analogue to memory-infra
// allocation register and needs to be merged/deduped.
class AllocationTracker : public MemlogReceiver {
 public:
  using CompleteCallback = base::OnceClosure;

  explicit AllocationTracker(CompleteCallback complete_cb);
  ~AllocationTracker() override;

  void OnHeader(const StreamHeader& header) override;
  void OnAlloc(const AllocPacket& alloc_packet,
               std::vector<Address>&& bt) override;
  void OnFree(const FreePacket& free_packet) override;
  void OnComplete() override;

 private:
  CompleteCallback complete_callback_;

  struct Alloc {
    Alloc(size_t sz, BacktraceStorage::Key key);

    size_t size;
    BacktraceStorage::Key backtrace_key;
  };

  // Cached pointer to the global singleton.
  BacktraceStorage* backtrace_storage_;

  std::map<Address, Alloc> live_allocs_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTracker);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_ALLOCATION_TRACKER_H_
