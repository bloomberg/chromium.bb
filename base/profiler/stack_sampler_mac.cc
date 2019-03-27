// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/thread_delegate_mac.h"

namespace base {

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    PlatformThreadId thread_id,
    ModuleCache* module_cache,
    StackSamplerTestDelegate* test_delegate) {
  return std::make_unique<StackSamplerImpl>(
      std::make_unique<ThreadDelegateMac>(thread_id, module_cache),
      module_cache, test_delegate);
}

// static
size_t StackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default macOS main thread stack size
  // of 8 MB (DFLSSIZ in <i386/vmparam.h>) with extra wiggle room.
  return stack_size > 0 ? stack_size : 12 * 1024 * 1024;
}

}  // namespace base
