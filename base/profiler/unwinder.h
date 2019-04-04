// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_UNWINDER_H_
#define BASE_PROFILER_UNWINDER_H_

#include <vector>

#include "base/profiler/frame.h"
#include "base/profiler/register_context.h"
#include "base/profiler/unwind_result.h"
#include "base/sampling_heap_profiler/module_cache.h"

namespace base {

// Unwinder provides an interface for stack frame unwinder implementations for
// use with the StackSamplingProfiler. The profiler is expected to call
// CanUnwind() to determine if the Unwinder thinks it can unwind from the frame
// represented by the context values, then TryUnwind() to attempt the
// unwind. Implementations of CanUnwindFrom() and TryUnwind() should be
// stateless because stack samples for all target threads use the same Unwinder
// instance.
class Unwinder {
 public:
  virtual ~Unwinder() = default;

  // Returns true if the unwinder recognizes the code referenced by
  // |current_frame| as code from which it should be able to unwind. When
  // multiple unwinders are in use, each should return true for a disjoint set
  // of frames. Note that if the unwinder returns true it may still legitmately
  // fail to unwind; e.g. in the case of a native unwind for a function that
  // doesn't have unwind information.
  virtual bool CanUnwindFrom(const Frame* current_frame) const = 0;

  // Attempts to unwind the frame represented by the context values. If
  // successful appends frames onto the stack and returns true. Otherwise
  // returns false.
  virtual UnwindResult TryUnwind(RegisterContext* thread_context,
                                 uintptr_t stack_top,
                                 ModuleCache* module_cache,
                                 std::vector<Frame>* stack) const = 0;

  Unwinder(const Unwinder&) = delete;
  Unwinder& operator=(const Unwinder&) = delete;

 protected:
  Unwinder() = default;
};

}  // namespace base

#endif  // BASE_PROFILER_UNWINDER_H_
