// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/compositor_app/mojo_context_provider.h"

#include "base/logging.h"

namespace mojo {
namespace examples {

MojoContextProvider::MojoContextProvider(ScopedMessagePipeHandle gl_pipe)
    : gl_pipe_(gl_pipe.Pass()) {}

bool MojoContextProvider::BindToCurrentThread() {
  DCHECK(gl_pipe_.is_valid());
  context_ = MojoGLES2CreateContext(
      gl_pipe_.release().value(), &ContextLostThunk, NULL, this);
  return !!context_;
}

gpu::gles2::GLES2Interface* MojoContextProvider::ContextGL() {
  if (!context_)
    return NULL;
  return static_cast<gpu::gles2::GLES2Interface*>(
      MojoGLES2GetGLES2Interface(context_));
}

gpu::ContextSupport* MojoContextProvider::ContextSupport() {
  if (!context_)
    return NULL;
  return static_cast<gpu::ContextSupport*>(
      MojoGLES2GetContextSupport(context_));
}

class GrContext* MojoContextProvider::GrContext() { return NULL; }

cc::ContextProvider::Capabilities MojoContextProvider::ContextCapabilities() {
  return capabilities_;
}

bool MojoContextProvider::IsContextLost() { return !context_; }
bool MojoContextProvider::DestroyedOnMainThread() { return !context_; }

MojoContextProvider::~MojoContextProvider() {
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void MojoContextProvider::ContextLost() {
  if (context_) {
    MojoGLES2DestroyContext(context_);
    context_ = NULL;
  }
}

}  // namespace examples
}  // namespace mojo
