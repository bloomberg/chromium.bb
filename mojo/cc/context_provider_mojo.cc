// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/context_provider_mojo.h"

#include "base/logging.h"
#include "mojo/public/cpp/environment/environment.h"

namespace mojo {

ContextProviderMojo::ContextProviderMojo(
    ScopedMessagePipeHandle command_buffer_handle)
    : command_buffer_handle_(command_buffer_handle.Pass()),
      context_lost_(false) {
}

bool ContextProviderMojo::BindToCurrentThread() {
  DCHECK(command_buffer_handle_.is_valid());
  context_ = MojoGLES2CreateContext(command_buffer_handle_.release().value(),
                                    &ContextLostThunk,
                                    this,
                                    Environment::GetDefaultAsyncWaiter());
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

bool ContextProviderMojo::IsContextLost() {
  return context_lost_;
}
bool ContextProviderMojo::DestroyedOnMainThread() { return !context_; }

ContextProviderMojo::~ContextProviderMojo() {
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void ContextProviderMojo::ContextLost() {
  context_lost_ = true;
}

}  // namespace mojo
