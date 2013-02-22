// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_context_provider.h"

#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

FakeContextProvider::FakeContextProvider()
    : destroyed_(false) {
}

FakeContextProvider::FakeContextProvider(
    const CreateCallback& create_callback)
    : create_callback_(create_callback),
      destroyed_(false) {
}

FakeContextProvider::~FakeContextProvider() {}

bool FakeContextProvider::InitializeOnMainThread() {
  if (destroyed_)
    return false;
  if (context3d_)
    return true;

  if (create_callback_.is_null())
    context3d_ = TestWebGraphicsContext3D::Create().Pass();
  else
    context3d_ = create_callback_.Run();
  destroyed_ = !context3d_;
  return !!context3d_;
}

bool FakeContextProvider::BindToCurrentThread() {
  return context3d_->makeContextCurrent();
}

WebKit::WebGraphicsContext3D* FakeContextProvider::Context3d() {
  return context3d_.get();
}
class GrContext* FakeContextProvider::GrContext() {
  // TODO(danakj): Make a fake GrContext.
  return NULL;
}

void FakeContextProvider::VerifyContexts() {
  if (!Context3d() || Context3d()->isContextLost()) {
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }
}

bool FakeContextProvider::DestroyedOnMainThread() {
  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

}  // namespace cc
