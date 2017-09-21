// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_FREED_OBJECT_TRACKER_H_
#define BASE_METRICS_FREED_OBJECT_TRACKER_H_

#include "base/macros.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/sharded_allocation_register.h"

namespace base {

template <typename T>
struct LazyInstanceTraitsBase;

namespace trace_event {

// Records what objects were freed so use-after-free knows where to look. This
// only tracks the last object freed from a given memory location so could be
// incorrect if too much time passes between being freed and being accessed.
// This is most accurate with BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS) set
// by 64-bit builds or "enable_profiling=true".
class BASE_EXPORT FreedObjectTracker {
 public:
  static FreedObjectTracker* GetInstance();

  void Enable();
  void DisableForTesting();

  // Gets information about an allocation. The |address| has to be an exact
  // match for the original allocation; no searching is done for a larger
  // allocation that encompasses that address. Returns false if no match
  // is found.
  bool Get(const void* address,
           AllocationRegister::Allocation* out_allocation) const;

  // This is public because it's called from the anonymous namespace.
  void RemoveAllocation(void* address);

 private:
  friend struct base::LazyInstanceTraitsBase<FreedObjectTracker>;

  FreedObjectTracker();
  ~FreedObjectTracker();

  ShardedAllocationRegister allocation_register_;

  DISALLOW_COPY_AND_ASSIGN(FreedObjectTracker);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_METRICS_FREED_OBJECT_TRACKER_H_
