// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate.h"

#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

GraphicsDelegate::GraphicsDelegate(const scoped_refptr<gl::GLSurface>& surface)
    : surface_(surface) {}

GraphicsDelegate::~GraphicsDelegate() {
  if (curr_context_id_ != NONE)
    contexts_[curr_context_id_]->ReleaseCurrent(surface_.get());
}

bool GraphicsDelegate::Initialize() {
  share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  for (auto& context : contexts_) {
    context = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                        gl::GLContextAttribs());
    if (!context.get()) {
      LOG(ERROR) << "gl::init::CreateGLContext failed";
      return false;
    }
  }
  return MakeMainContextCurrent();
}

bool GraphicsDelegate::MakeMainContextCurrent() {
  return MakeContextCurrent(MAIN_CONTEXT);
}

bool GraphicsDelegate::MakeSkiaContextCurrent() {
  return MakeContextCurrent(SKIA_CONTEXT);
}

bool GraphicsDelegate::MakeContextCurrent(ContextId context_id) {
  DCHECK(context_id > NONE && context_id < NUM_CONTEXTS);
  if (curr_context_id_ == context_id)
    return true;
  auto& context = contexts_[context_id];
  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }
  return true;
}

}  // namespace vr
