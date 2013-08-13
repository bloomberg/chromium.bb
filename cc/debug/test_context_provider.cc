// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/test_context_provider.h"

#include "base/logging.h"
#include "cc/debug/test_web_graphics_context_3d.h"

namespace cc {

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create() {
  return Create(TestWebGraphicsContext3D::CreateFactory());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    const CreateCallback& create_callback) {
  scoped_refptr<TestContextProvider> provider = new TestContextProvider;
  if (!provider->InitializeOnMainThread(create_callback))
    return NULL;
  return provider;
}

TestContextProvider::TestContextProvider()
    : bound_(false),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

TestContextProvider::~TestContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool TestContextProvider::InitializeOnMainThread(
    const CreateCallback& create_callback) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  DCHECK(!context3d_);
  DCHECK(!create_callback.is_null());
  context3d_ = create_callback.Run();
  return context3d_;
}

bool TestContextProvider::BindToCurrentThread() {
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

WebKit::WebGraphicsContext3D* TestContextProvider::Context3d() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

class GrContext* TestContextProvider::GrContext() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  // TODO(danakj): Make a test GrContext that works with a test Context3d.
  return NULL;
}

void TestContextProvider::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }
}

bool TestContextProvider::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void TestContextProvider::SetLostContextCallback(
    const LostContextCallback& cb) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace cc
