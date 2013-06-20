// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"

namespace cc {

base::SingleThreadTaskRunner* Proxy::MainThreadTaskRunner() const {
  return main_task_runner_.get();
}

bool Proxy::HasImplThread() const { return !!impl_task_runner_.get(); }

base::SingleThreadTaskRunner* Proxy::ImplThreadTaskRunner() const {
  return impl_task_runner_.get();
}

bool Proxy::IsMainThread() const {
#ifndef NDEBUG
  DCHECK(main_task_runner_.get());
  if (impl_thread_is_overridden_)
    return false;
  return main_task_runner_->BelongsToCurrentThread();
#else
  return true;
#endif
}

bool Proxy::IsImplThread() const {
#ifndef NDEBUG
  if (impl_thread_is_overridden_)
    return true;
  if (!impl_task_runner_.get())
    return false;
  return impl_task_runner_->BelongsToCurrentThread();
#else
  return true;
#endif
}

#ifndef NDEBUG
void Proxy::SetCurrentThreadIsImplThread(bool is_impl_thread) {
  impl_thread_is_overridden_ = is_impl_thread;
}
#endif

bool Proxy::IsMainThreadBlocked() const {
#ifndef NDEBUG
  return is_main_thread_blocked_;
#else
  return true;
#endif
}

#ifndef NDEBUG
void Proxy::SetMainThreadBlocked(bool is_main_thread_blocked) {
  is_main_thread_blocked_ = is_main_thread_blocked;
}
#endif

Proxy::Proxy(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
    : main_task_runner_(base::MessageLoopProxy::current()),
#ifdef NDEBUG
      impl_task_runner_(impl_task_runner) {}
#else
      impl_task_runner_(impl_task_runner),
      impl_thread_is_overridden_(false),
      is_main_thread_blocked_(false) {}
#endif

Proxy::~Proxy() {
  DCHECK(IsMainThread());
}

std::string Proxy::SchedulerStateAsStringForTesting() {
  return "";
}

}  // namespace cc
