// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_UNWIND_RESULT_H_
#define BASE_PROFILER_UNWIND_RESULT_H_

namespace base {

// The result of attempting to unwind stack frames.
enum class UnwindResult {
  // The end of the stack was reached successfully.
  COMPLETED,

  // The walk reached a frame that it doesn't know how to unwind, but might be
  // unwindable by the other native/aux unwinder.
  UNRECOGNIZED_FRAME,

  // The walk was aborted and is not resumable.
  ABORTED,
};

}  // namespace base

#endif  // BASE_PROFILER_UNWIND_RESULT_H_
