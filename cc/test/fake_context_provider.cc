// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_context_provider.h"

#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

FakeContextProvider::FakeContextProvider()
    : bound_(false),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

FakeContextProvider::~FakeContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool FakeContextProvider::InitializeOnMainThread(
    const CreateCallback& create_callback) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  DCHECK(!context3d_);
  if (create_callback.is_null())
    context3d_ = TestWebGraphicsContext3D::Create().Pass();
  else
    context3d_ = create_callback.Run();
  return context3d_;
}

bool FakeContextProvider::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bound_)
    return true;

  bound_ = true;
  if (!context3d_->makeContextCurrent()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
    return false;
  }
  return true;
}

WebKit::WebGraphicsContext3D* FakeContextProvider::Context3d() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}
class GrContext* FakeContextProvider::GrContext() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  // TODO(danakj): Make a fake GrContext.
  return NULL;
}

void FakeContextProvider::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }
}

bool FakeContextProvider::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void FakeContextProvider::SetLostContextCallback(
    const LostContextCallback& cb) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace cc
