// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/context_provider_mojo.h"

#include "base/logging.h"

namespace mojo {

ContextProviderMojo::ContextProviderMojo(ScopedMessagePipeHandle gl_pipe)
    : gl_pipe_(gl_pipe.Pass()) {}

bool ContextProviderMojo::BindToCurrentThread() {
  DCHECK(gl_pipe_.is_valid());
  context_ = MojoGLES2CreateContext(
      gl_pipe_.release().value(), &ContextLostThunk, NULL, this);
  return !!context_;
}

gpu::gles2::GLES2Interface* ContextProviderMojo::ContextGL() {
  if (!context_)
    return NULL;
  return static_cast<gpu::gles2::GLES2Interface*>(
      MojoGLES2GetGLES2Interface(context_));
}

gpu::ContextSupport* ContextProviderMojo::ContextSupport() {
  if (!context_)
    return NULL;
  return static_cast<gpu::ContextSupport*>(
      MojoGLES2GetContextSupport(context_));
}

class GrContext* ContextProviderMojo::GrContext() { return NULL; }

cc::ContextProvider::Capabilities ContextProviderMojo::ContextCapabilities() {
  return capabilities_;
}

bool ContextProviderMojo::IsContextLost() { return !context_; }
bool ContextProviderMojo::DestroyedOnMainThread() { return !context_; }

ContextProviderMojo::~ContextProviderMojo() {
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void ContextProviderMojo::ContextLost() {
  if (context_) {
    MojoGLES2DestroyContext(context_);
    context_ = NULL;
  }
}

}  // namespace mojo
