// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proxy.h"

#include "cc/thread.h"
#include "cc/thread_impl.h"

namespace cc {

Thread* Proxy::MainThread() const { return main_thread_.get(); }

bool Proxy::HasImplThread() const { return impl_thread_; }

Thread* Proxy::ImplThread() const { return impl_thread_.get(); }

Thread* Proxy::CurrentThread() const {
  if (MainThread() && MainThread()->belongsToCurrentThread())
    return MainThread();
  if (ImplThread() && ImplThread()->belongsToCurrentThread())
    return ImplThread();
  return NULL;
}

bool Proxy::IsMainThread() const {
#ifndef NDEBUG
  DCHECK(MainThread());
  if (impl_thread_is_overridden_)
    return false;
  return MainThread()->belongsToCurrentThread();
#else
  return true;
#endif
}

bool Proxy::IsImplThread() const {
#ifndef NDEBUG
  if (impl_thread_is_overridden_)
    return true;
  return ImplThread() && ImplThread()->belongsToCurrentThread();
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

Proxy::Proxy(scoped_ptr<Thread> impl_thread)
    : main_thread_(ThreadImpl::createForCurrentThread()),
#ifdef NDEBUG
      impl_thread_(impl_thread.Pass()) {}
#else
      impl_thread_(impl_thread.Pass()),
      impl_thread_is_overridden_(false),
      is_main_thread_blocked_(false) {}
#endif

Proxy::~Proxy() {
  DCHECK(IsMainThread());
}

}  // namespace cc
