// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
#define MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

// This is a helper utility for base::Bind()ing callbacks to the current
// MessageLoop. The typical use is when |a| (of class |A|) wants to hand a
// callback such as base::Bind(&A::AMethod, a) to |b|, but needs to ensure that
// when |b| executes the callback, it does so on |a|'s current MessageLoop.
//
// Typical usage: request to be called back on the current thread:
// other->StartAsyncProcessAndCallMeBack(
//    media::BindToCurrentLoop(base::Bind(&MyClass::MyMethod, this)));

namespace media {
namespace internal {

// First, tell the compiler TrampolineHelper is a struct template with one
// type parameter.  Then define specializations where the type is a function
// returning void and taking zero or more arguments.
template <typename Signature>
class TrampolineHelper;

template <typename... Args>
class TrampolineHelper<void(Args...)> {
 public:
  using CallbackType = base::Callback<void(Args...)>;

  TrampolineHelper(const tracked_objects::Location& posted_from,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   CallbackType callback)
      : posted_from_(posted_from),
        task_runner_(std::move(task_runner)),
        callback_(std::move(callback)) {
    DCHECK(task_runner_);
    DCHECK(callback_);
  }

  inline void Run(Args... args);

  ~TrampolineHelper() {
    task_runner_->PostTask(
        posted_from_,
        base::Bind(&TrampolineHelper::ClearCallbackOnTargetTaskRunner,
                   base::Passed(&callback_)));
  }

 private:
  static void ClearCallbackOnTargetTaskRunner(CallbackType) {}
  static void RunOnceClosure(base::OnceClosure cb) { std::move(cb).Run(); }

  tracked_objects::Location posted_from_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  CallbackType callback_;
};

template <>
inline void TrampolineHelper<void()>::Run() {
  task_runner_->PostTask(posted_from_, callback_);
}

template <typename... Args>
inline void TrampolineHelper<void(Args...)>::Run(Args... args) {
  // TODO(tzik): Use OnceCallback directly without RunOnceClosure, once
  // TaskRunner::PostTask migrates to OnceClosure.
  base::OnceClosure cb = base::BindOnce(callback_, std::forward<Args>(args)...);
  task_runner_->PostTask(
      posted_from_,
      base::Bind(&TrampolineHelper::RunOnceClosure, base::Passed(&cb)));
}

}  // namespace internal

template <typename T>
inline base::Callback<T> BindToCurrentLoop(base::Callback<T> cb) {
  return base::Bind(
      &internal::TrampolineHelper<T>::Run,
      base::MakeUnique<internal::TrampolineHelper<T>>(
          FROM_HERE,  // TODO(tzik): Propagate FROM_HERE from the caller.
          base::ThreadTaskRunnerHandle::Get(), std::move(cb)));
}

}  // namespace media

#endif  // MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
