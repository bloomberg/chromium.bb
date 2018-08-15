// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/base_compositor_delegate.h"

#include <utility>

#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

BaseCompositorDelegate::BaseCompositorDelegate() {}

BaseCompositorDelegate::~BaseCompositorDelegate() {
  if (curr_context_id_ != kNone)
    contexts_[curr_context_id_]->ReleaseCurrent(surface_.get());
}

bool BaseCompositorDelegate::Initialize(
    const scoped_refptr<gl::GLSurface>& surface) {
  surface_ = surface;
  share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  for (auto& context : contexts_) {
    context = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                        gl::GLContextAttribs());
    if (!context.get()) {
      LOG(ERROR) << "gl::init::CreateGLContext failed";
      return false;
    }
  }
  return MakeContextCurrent(kMainContext);
}

bool BaseCompositorDelegate::RunInSkiaContext(SkiaContextCallback callback) {
  DCHECK_EQ(curr_context_id_, kMainContext);
  if (!MakeContextCurrent(kSkiaContext))
    return false;
  std::move(callback).Run();
  return MakeContextCurrent(kMainContext);
}

bool BaseCompositorDelegate::MakeContextCurrent(ContextId context_id) {
  DCHECK(context_id > kNone && context_id < kNumContexts);
  if (curr_context_id_ == context_id)
    return true;
  auto& context = contexts_[context_id];
  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }
  curr_context_id_ = context_id;
  return true;
}

}  // namespace vr
