// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/context_provider_mojo.h"

#include "base/logging.h"
#include "mojo/gpu/mojo_gles2_impl_autogen.h"
#include "third_party/mojo/src/mojo/public/cpp/environment/environment.h"

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
  context_gl_.reset(new MojoGLES2Impl(context_));
  return !!context_;
}

gpu::gles2::GLES2Interface* ContextProviderMojo::ContextGL() {
  return context_gl_.get();
}

gpu::ContextSupport* ContextProviderMojo::ContextSupport() {
  if (!context_)
    return NULL;
  return static_cast<gpu::ContextSupport*>(
      MojoGLES2GetContextSupport(context_));
}

class GrContext* ContextProviderMojo::GrContext() { return NULL; }

void ContextProviderMojo::InvalidateGrContext(uint32_t state) {
}

cc::ContextProvider::Capabilities ContextProviderMojo::ContextCapabilities() {
  return capabilities_;
}

void ContextProviderMojo::SetupLock() {
}

base::Lock* ContextProviderMojo::GetLock() {
  return &context_lock_;
}

bool ContextProviderMojo::IsContextLost() {
  return context_lost_;
}
bool ContextProviderMojo::DestroyedOnMainThread() { return !context_; }

ContextProviderMojo::~ContextProviderMojo() {
  context_gl_.reset();
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void ContextProviderMojo::ContextLost() {
  context_lost_ = true;
}

}  // namespace mojo
