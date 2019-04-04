// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_NATIVE_UNWINDER_MAC_H_
#define BASE_PROFILER_NATIVE_UNWINDER_MAC_H_

#include "base/macros.h"
#include "base/profiler/unwinder.h"

namespace base {

// Native unwinder implementation for Mac, using libunwind.
class NativeUnwinderMac : public Unwinder {
 public:
  NativeUnwinderMac(ModuleCache* module_cache);

  NativeUnwinderMac(const NativeUnwinderMac&) = delete;
  NativeUnwinderMac& operator=(const NativeUnwinderMac&) = delete;

  // Unwinder:
  bool CanUnwindFrom(const Frame* current_frame) const override;
  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         ModuleCache* module_cache,
                         std::vector<Frame>* stack) const override;

 private:
  // Cached pointer to the libsystem_kernel module.
  const ModuleCache::Module* const libsystem_kernel_module_;

  // The address range of |_sigtramp|, the signal trampoline function.
  uintptr_t sigtramp_start_;
  uintptr_t sigtramp_end_;
};

}  // namespace base

#endif  // BASE_PROFILER_NATIVE_UNWINDER_MAC_H_
