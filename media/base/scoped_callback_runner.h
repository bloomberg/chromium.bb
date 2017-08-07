// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_CALLBACK_RUNNER_H_
#define MEDIA_BASE_SCOPED_CALLBACK_RUNNER_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"

// This is a helper utility to wrap a base::OnceCallback such that if the
// callback is destructed before it has a chance to run (e.g. the callback is
// bound into a task and the task is dropped), it will be run with the
// default arguments passed into ScopedCallbackRunner.
//
// Example:
//   foo->DoWorkAndReturnResult(
//     ScopedCallbackRunner(base::BindOnce(&Foo::OnResult, this), false));
//
// If the callback is destructed without running, it'll be run with "false".

namespace media {
namespace internal {

// First, tell the compiler ScopedCallbackRunnerHelper is a class template with
// one type parameter. Then define specializations where the type is a function
// returning void and taking zero or more arguments.
template <typename Signature>
class ScopedCallbackRunnerHelper;

// Only support callbacks that return void because otherwise it is odd to call
// the callback in the destructor and drop the return value immediately.
template <typename... Args>
class ScopedCallbackRunnerHelper<void(Args...)> {
 public:
  using CallbackType = base::OnceCallback<void(Args...)>;

  // Bound arguments may be different to the callback signature when wrappers
  // are used, e.g. in base::Owned and base::Unretained case, they are
  // OwnedWrapper and UnretainedWrapper. Use BoundArgs to help handle this.
  template <typename... BoundArgs>
  ScopedCallbackRunnerHelper(CallbackType callback, BoundArgs&&... args)
      : callback_(std::move(callback)) {
    destruction_callback_ =
        base::BindOnce(&ScopedCallbackRunnerHelper::Run, base::Unretained(this),
                       std::forward<BoundArgs>(args)...);
  }

  ~ScopedCallbackRunnerHelper() {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  void Run(Args... args) {
    destruction_callback_.Reset();
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  CallbackType callback_;
  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCallbackRunnerHelper);
};

}  // namespace internal

// Currently ScopedCallbackRunner only supports base::OnceCallback. If needed,
// we can easily add a specialization to support base::RepeatingCallback too.
template <typename T, typename... Args>
inline base::OnceCallback<T> ScopedCallbackRunner(base::OnceCallback<T> cb,
                                                  Args&&... args) {
  return base::BindOnce(
      &internal::ScopedCallbackRunnerHelper<T>::Run,
      base::MakeUnique<internal::ScopedCallbackRunnerHelper<T>>(
          std::move(cb), std::forward<Args>(args)...));
}

}  // namespace media

#endif  // MEDIA_BASE_SCOPED_CALLBACK_RUNNER_H_
