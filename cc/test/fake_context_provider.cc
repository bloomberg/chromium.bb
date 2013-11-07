// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_context_provider.h"

#include "base/bind.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

// static
scoped_refptr<FakeContextProvider> FakeContextProvider::
    CreateForOtherThread() {
  scoped_refptr<FakeContextProvider> provider = new FakeContextProvider();
  if (!provider->InitializeOnMainThread())
    return NULL;
  return provider;
}

static scoped_ptr<TestWebGraphicsContext3D> ReturnContext(
    scoped_ptr<TestWebGraphicsContext3D> context3d) {
  return context3d.Pass();
}

// static
scoped_refptr<FakeContextProvider> FakeContextProvider::CreateForOtherThread(
    scoped_ptr<TestWebGraphicsContext3D> context3d) {
  CreateCallback create_callback =
      base::Bind(&ReturnContext, base::Passed(&context3d));
  scoped_refptr<FakeContextProvider> provider =
      new FakeContextProvider(create_callback);
  if (!provider->InitializeOnMainThread())
    return NULL;
  return provider;
}

// static
scoped_refptr<FakeContextProvider> FakeContextProvider::CreateForOtherThread(
    const CreateCallback& create_callback) {
  scoped_refptr<FakeContextProvider> provider =
      new FakeContextProvider(create_callback);
  if (!provider->InitializeOnMainThread())
    return NULL;
  return provider;
}

FakeContextProvider::FakeContextProvider()
    : destroyed_(false) {
}

FakeContextProvider::FakeContextProvider(
    const CreateCallback& create_callback)
    : create_callback_(create_callback),
      bound_(false),
      destroyed_(false) {
}

FakeContextProvider::~FakeContextProvider() {}

bool FakeContextProvider::InitializeOnMainThread() {
  DCHECK(!context3d_);

  if (create_callback_.is_null())
    context3d_ = TestWebGraphicsContext3D::Create().Pass();
  else
    context3d_ = create_callback_.Run();
  return context3d_;
}

bool FakeContextProvider::BindToCurrentThread() {
  bound_ = true;
  if (!context3d_->makeContextCurrent()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
    return false;
  }
  return true;
}

blink::WebGraphicsContext3D* FakeContextProvider::Context3d() {
  DCHECK(context3d_);
  DCHECK(bound_);

  return context3d_.get();
}
class GrContext* FakeContextProvider::GrContext() {
  DCHECK(context3d_);
  DCHECK(bound_);

  // TODO(danakj): Make a fake GrContext.
  return NULL;
}

void FakeContextProvider::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(bound_);

  if (context3d_->isContextLost()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }
}

bool FakeContextProvider::DestroyedOnMainThread() {
  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

}  // namespace cc
