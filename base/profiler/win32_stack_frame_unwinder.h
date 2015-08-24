// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_
#define BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_

#include <windows.h>

#include "base/base_export.h"
#include "base/macros.h"

namespace base {

#if !defined(_WIN64)
// Allows code to compile for x86. Actual support for x86 will require either
// refactoring these interfaces or separate architecture-specific interfaces.
using PRUNTIME_FUNCTION = void*;
#endif  // !defined(_WIN64)

// Instances of this class are expected to be created and destroyed for each
// stack unwinding, outside of SuspendThread/ResumeThread.
class BASE_EXPORT Win32StackFrameUnwinder {
 public:
  // Interface for Win32 unwind-related functionality this class depends
  // on. Provides a seam for testing.
  class BASE_EXPORT UnwindFunctions {
   public:
    virtual ~UnwindFunctions();

    virtual PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                                  PDWORD64 image_base) = 0;
    virtual void VirtualUnwind(DWORD64 image_base,
                               DWORD64 program_counter,
                               PRUNTIME_FUNCTION runtime_function,
                               CONTEXT* context) = 0;
   protected:
    UnwindFunctions();

   private:
    DISALLOW_COPY_AND_ASSIGN(UnwindFunctions);
  };

  class BASE_EXPORT Win32UnwindFunctions : public UnwindFunctions {
   public:
    Win32UnwindFunctions();

    PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                          PDWORD64 image_base) override;

    void VirtualUnwind(DWORD64 image_base,
                       DWORD64 program_counter,
                       PRUNTIME_FUNCTION runtime_function,
                       CONTEXT* context) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(Win32UnwindFunctions);
  };

  Win32StackFrameUnwinder();
  ~Win32StackFrameUnwinder();

  bool TryUnwind(CONTEXT* context);

 private:
  // This function is for test purposes only.
  Win32StackFrameUnwinder(UnwindFunctions* unwind_functions);
  friend class Win32StackFrameUnwinderTest;

  // State associated with each stack unwinding.
  bool at_top_frame_;
  bool unwind_info_present_for_all_frames_;
  const void* pending_blacklisted_module_;

  Win32UnwindFunctions win32_unwind_functions_;
  UnwindFunctions* const unwind_functions_;

  DISALLOW_COPY_AND_ASSIGN(Win32StackFrameUnwinder);
};

}  // namespace base

#endif  // BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_
