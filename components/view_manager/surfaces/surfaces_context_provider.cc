// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/surfaces_context_provider.h"

#include "base/logging.h"
#include "third_party/mojo/src/mojo/public/cpp/environment/environment.h"

namespace surfaces {

SurfacesContextProvider::SurfacesContextProvider(
    mojo::ScopedMessagePipeHandle command_buffer_handle)
    : command_buffer_handle_(command_buffer_handle.Pass()), context_(nullptr) {
  capabilities_.gpu.image = true;
}

bool SurfacesContextProvider::BindToCurrentThread() {
  DCHECK(command_buffer_handle_.is_valid());
  context_ = MojoGLES2CreateContext(command_buffer_handle_.release().value(),
                                    &ContextLostThunk, this,
                                    mojo::Environment::GetDefaultAsyncWaiter());
  DCHECK(context_);
  return !!context_;
}

gpu::gles2::GLES2Interface* SurfacesContextProvider::ContextGL() {
  if (!context_)
    return nullptr;
  return static_cast<gpu::gles2::GLES2Interface*>(
      MojoGLES2GetGLES2Interface(context_));
}

gpu::ContextSupport* SurfacesContextProvider::ContextSupport() {
  if (!context_)
    return nullptr;
  return static_cast<gpu::ContextSupport*>(
      MojoGLES2GetContextSupport(context_));
}

class GrContext* SurfacesContextProvider::GrContext() {
  return NULL;
}

void SurfacesContextProvider::InvalidateGrContext(uint32_t state) {
}

cc::ContextProvider::Capabilities
SurfacesContextProvider::ContextCapabilities() {
  return capabilities_;
}

void SurfacesContextProvider::SetupLock() {
}

base::Lock* SurfacesContextProvider::GetLock() {
  return &context_lock_;
}

bool SurfacesContextProvider::DestroyedOnMainThread() {
  return !context_;
}

void SurfacesContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  lost_context_callback_ = lost_context_callback;
}

SurfacesContextProvider::~SurfacesContextProvider() {
  if (context_)
    MojoGLES2DestroyContext(context_);
}

void SurfacesContextProvider::ContextLost() {
  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
}

}  // namespace surfaces
