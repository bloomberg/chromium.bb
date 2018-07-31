// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_STACK_UNWINDER_ANDROID_H_
#define COMPONENTS_TRACING_COMMON_STACK_UNWINDER_ANDROID_H_

#include <map>

#include "base/debug/proc_maps_linux.h"
#include "base/threading/platform_thread.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// Utility to unwind stacks for current thread on ARM devices. Contains ability
// to unwind stacks based on EHABI section in Android libraries and using the
// custom stack unwind information in Chrome. This works on top of
// base::trace_event::CFIBacktraceAndroid, which unwinds Chrome only stacks.
class TRACING_EXPORT StackUnwinderAndroid {
 public:
  static StackUnwinderAndroid* GetInstance();

  // Intializes the unwinder for current process. It finds all loaded libraries
  // in current process and also initializes CFIBacktraceAndroid, with file IO.
  void Initialize();

  // Unwinds stack frames for current thread and stores the program counters in
  // |out_trace|, and returns the number of frames stored.
  size_t TraceStack(const void** out_trace, size_t max_depth);

  // Same as above function, but pauses the thread with the given |tid| and then
  // unwinds. |tid| should not be current thread's.
  size_t TraceStack(base::PlatformThreadId tid,
                    const void** out_trace,
                    size_t max_depth);

  // Returns the end address of the memory map with given |addr|.
  uintptr_t GetEndAddressOfRegion(uintptr_t addr) const;

  // Returns true if the given |pc| was part of any mapped segments in the
  // process.
  bool IsAddressMapped(uintptr_t pc) const;

 private:
  StackUnwinderAndroid();
  ~StackUnwinderAndroid();

  bool is_initialized_ = false;

  // Stores all the memory mapped regions in the current process, including all
  // the files mapped and anonymous regions. This data could be stale, but the
  // error caused by changes in library loads would be missing stackframes and
  // is acceptable.
  std::vector<base::debug::MappedMemoryRegion> regions_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_STACK_UNWINDER_ANDROID_H_
