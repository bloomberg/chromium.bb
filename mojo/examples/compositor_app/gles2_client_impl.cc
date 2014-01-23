// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/compositor_app/gles2_client_impl.h"

#include "base/debug/trace_event.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace mojo {
namespace examples {

GLES2ClientImpl::GLES2ClientImpl(
    ScopedMessagePipeHandle pipe,
    const base::Callback<void(gfx::Size)>& context_created_callback)
    : context_created_callback_(context_created_callback) {
  context_ = MojoGLES2CreateContext(
      pipe.release().value(),
      &DidCreateContextThunk,
      &ContextLostThunk,
      NULL,
      this);
}

GLES2ClientImpl::~GLES2ClientImpl() {
  if (context_)
    MojoGLES2DestroyContext(context_);
}

gpu::gles2::GLES2Interface* GLES2ClientImpl::Interface() const {
  if (!context_)
    return NULL;
  return static_cast<gpu::gles2::GLES2Interface*>(
      MojoGLES2GetGLES2Interface(context_));
}

gpu::ContextSupport* GLES2ClientImpl::Support() const {
  if (!context_)
    return NULL;
  return static_cast<gpu::ContextSupport*>(
      MojoGLES2GetContextSupport(context_));
}

void GLES2ClientImpl::DidCreateContext(uint32_t width,
                                       uint32_t height) {
  TRACE_EVENT0("compositor_app", "DidCreateContext");
  if (!context_created_callback_.is_null())
    context_created_callback_.Run(gfx::Size(width, height));
}

void GLES2ClientImpl::DidCreateContextThunk(
    void* closure,
    uint32_t width,
    uint32_t height) {
  static_cast<GLES2ClientImpl*>(closure)->DidCreateContext(width, height);
}

void GLES2ClientImpl::ContextLost() {
  if (context_) {
    MojoGLES2DestroyContext(context_);
    context_ = NULL;
  }
}

void GLES2ClientImpl::ContextLostThunk(void* closure) {
  static_cast<GLES2ClientImpl*>(closure)->ContextLost();
}

}  // namespace examples
}  // namespace mojo
