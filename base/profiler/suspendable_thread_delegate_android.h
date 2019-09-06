// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_SUSPENDABLE_THREAD_DELEGATE_ANDROID_H_
#define BASE_PROFILER_SUSPENDABLE_THREAD_DELEGATE_ANDROID_H_

#include "base/base_export.h"
#include "base/profiler/suspendable_thread_delegate.h"

namespace base {

// Platform- and thread-specific implementation in support of stack sampling on
// Android.
//
// TODO(https://crbug.com/988579): Inherit from ThreadDelegate rather than
// SuspendableThreadDelegate. Implement this class.
class BASE_EXPORT SuspendableThreadDelegateAndroid
    : public SuspendableThreadDelegate {
 public:
  class ScopedSuspendThread
      : public SuspendableThreadDelegate::ScopedSuspendThread {
   public:
    ScopedSuspendThread() = default;
    ~ScopedSuspendThread() override = default;

    ScopedSuspendThread(const ScopedSuspendThread&) = delete;
    ScopedSuspendThread& operator=(const ScopedSuspendThread&) = delete;

    bool WasSuccessful() const override;
  };

  SuspendableThreadDelegateAndroid() = default;
  ~SuspendableThreadDelegateAndroid() override = default;

  SuspendableThreadDelegateAndroid(const SuspendableThreadDelegateAndroid&) =
      delete;
  SuspendableThreadDelegateAndroid& operator=(
      const SuspendableThreadDelegateAndroid&) = delete;

  // SuspendableThreadDelegate
  std::unique_ptr<SuspendableThreadDelegate::ScopedSuspendThread>
  CreateScopedSuspendThread() override;
  bool GetThreadContext(RegisterContext* thread_context) override;
  uintptr_t GetStackBaseAddress() const override;
  bool CanCopyStack(uintptr_t stack_pointer) override;
  std::vector<uintptr_t*> GetRegistersToRewrite(
      RegisterContext* thread_context) override;
};

}  // namespace base

#endif  // BASE_PROFILER_SUSPENDABLE_THREAD_DELEGATE_ANDROID_H_
