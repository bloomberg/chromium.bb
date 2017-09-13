// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_ALLOCATION_TRACKER_H_
#define CHROME_PROFILING_ALLOCATION_TRACKER_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/profiling/allocation_event.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/memlog_receiver.h"

namespace profiling {

// Tracks live allocations in one process. This is an analogue to memory-infra
// allocation register and needs to be merged/deduped.
class AllocationTracker : public MemlogReceiver {
 public:
  using CompleteCallback = base::OnceClosure;
  using ContextMap = std::map<std::string, int>;

  AllocationTracker(CompleteCallback complete_cb,
                    BacktraceStorage* backtrace_storage);
  ~AllocationTracker() override;

  void OnHeader(const StreamHeader& header) override;
  void OnAlloc(const AllocPacket& alloc_packet,
               std::vector<Address>&& bt,
               std::string&& context) override;
  void OnFree(const FreePacket& free_packet) override;
  void OnComplete() override;

  const AllocationEventSet& live_allocs() const { return live_allocs_; }
  const ContextMap& context() const { return context_; }

  // Returns the aggregated allocation counts currently live.
  AllocationCountMap GetCounts() const;

 private:
  CompleteCallback complete_callback_;

  BacktraceStorage* backtrace_storage_;

  // Need to track all live objects. Since the free information doesn't have
  // the metadata, we can't keep a map of counts indexed by just the metadata
  // (which is all the trace JSON needs), but need to keep an index by address.
  //
  // This could be a two-level index, where one set of metadata is kept and
  // addresses index into that. But a full copy of the metadata is about the
  // same size as the internal map node required for this second index, with
  // additional complexity.
  AllocationEventSet live_allocs_;

  // The context strings are atoms. Since there are O(100's) of these, we do
  // not bother to uniquify across all processes. This map contains the unique
  // context strings for the current process, mapped to the unique ID for that
  // context. This unique ID is stored in the allocation. This is optimized for
  // insertion. When querying, a reverse index will need to be generated
  ContextMap context_;
  int next_context_id_ = 1;  // 0 means no context.

  DISALLOW_COPY_AND_ASSIGN(AllocationTracker);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_ALLOCATION_TRACKER_H_
