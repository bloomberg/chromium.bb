// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/gles2/gles2_impl.h"

#include "base/bind.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

namespace mojo {
namespace services {

GLES2Impl::GLES2Impl(ScopedMessagePipeHandle client)
    : client_(client.Pass()) {
  client_.SetPeer(this);
}

GLES2Impl::~GLES2Impl() {
}

void GLES2Impl::Destroy() {
  gl_context_.reset();
}

void GLES2Impl::CreateContext(gfx::AcceleratedWidget widget,
                              const gfx::Size& size) {
  gpu::GLInProcessContextAttribs attribs;
  gl_context_.reset(gpu::GLInProcessContext::CreateContext(
      false, widget, size, false, attribs, gfx::PreferDiscreteGpu));
  gl_context_->SetContextLostCallback(base::Bind(
      &GLES2Impl::OnGLContextLost, base::Unretained(this)));

  gpu::gles2::GLES2Interface* gl = gl_context_->GetImplementation();
  uint64_t encoded_gl = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gl));

  client_->DidCreateContext(encoded_gl, size.width(), size.height());
}

void GLES2Impl::OnGLContextLost() {
  client_->ContextLost();
}

}  // namespace services
}  // namespace mojo
