// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/context_provider.h"

#include <stdint.h>

#include "base/logging.h"
#include "mojo/gles2/gles2_context.h"
#include "mojo/gpu/mojo_gles2_impl_autogen.h"

namespace mus {

ContextProvider::ContextProvider(
    mojo::ScopedMessagePipeHandle command_buffer_handle)
    : command_buffer_handle_(std::move(command_buffer_handle)),
      context_(nullptr) {
}

bool ContextProvider::BindToCurrentThread() {
  DCHECK(command_buffer_handle_.is_valid());
  context_ = MojoGLES2CreateContext(command_buffer_handle_.release().value(),
                                    nullptr, &ContextLostThunk, this);
  context_gl_.reset(new mojo::MojoGLES2Impl(context_));
  return !!context_;
}

gpu::gles2::GLES2Interface* ContextProvider::ContextGL() {
  return context_gl_.get();
}

gpu::ContextSupport* ContextProvider::ContextSupport() {
  if (!context_)
    return NULL;
  // TODO(rjkroege): Ensure that UIP does not take this code path.
  return static_cast<gles2::GLES2Context*>(context_)->context_support();
}

class GrContext* ContextProvider::GrContext() {
  return NULL;
}

void ContextProvider::InvalidateGrContext(uint32_t state) {}

gpu::Capabilities ContextProvider::ContextCapabilities() {
  gpu::Capabilities capabilities;
  // Enabled the CHROMIUM_image extension to use GpuMemoryBuffers. The
  // implementation of which is used in CommandBufferDriver.
  capabilities.image = true;
  return capabilities;
}

void ContextProvider::SetupLock() {}

base::Lock* ContextProvider::GetLock() {
  return &context_lock_;
}

ContextProvider::~ContextProvider() {
  context_gl_.reset();
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void ContextProvider::ContextLost() {}

}  // namespace mus
