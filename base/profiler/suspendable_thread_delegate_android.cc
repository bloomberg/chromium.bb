// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/suspendable_thread_delegate_android.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

// ScopedSuspendThread --------------------------------------------------------

bool SuspendableThreadDelegateAndroid::ScopedSuspendThread::WasSuccessful()
    const {
  return false;
}

// SuspendableThreadDelegateAndroid -------------------------------------------

std::unique_ptr<SuspendableThreadDelegate::ScopedSuspendThread>
SuspendableThreadDelegateAndroid::CreateScopedSuspendThread() {
  return std::make_unique<ScopedSuspendThread>();
}

// NO HEAP ALLOCATIONS.
bool SuspendableThreadDelegateAndroid::GetThreadContext(
    RegisterContext* thread_context) {
  return false;
}

// NO HEAP ALLOCATIONS.
uintptr_t SuspendableThreadDelegateAndroid::GetStackBaseAddress() const {
  // It's okay for the stub to return zero here: GetStackBaseAddress() if
  // ScopedSuspendThread fails, which it always will in the stub.
  return 0;
}

// NO HEAP ALLOCATIONS.
bool SuspendableThreadDelegateAndroid::CanCopyStack(uintptr_t stack_pointer) {
  return false;
}

std::vector<uintptr_t*> SuspendableThreadDelegateAndroid::GetRegistersToRewrite(
    RegisterContext* thread_context) {
  return {};
}

}  // namespace base
