// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/compositor_app/gles2_client_impl.h"

#include "base/debug/trace_event.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "mojo/public/gles2/gles2.h"

namespace mojo {
namespace examples {

GLES2ClientImpl::GLES2ClientImpl(
    ScopedMessagePipeHandle pipe,
    const base::Callback<void(gfx::Size)>& context_created_callback)
    : context_created_callback_(context_created_callback),
      service_(pipe.Pass()) {
  service_.SetPeer(this);
}

GLES2ClientImpl::~GLES2ClientImpl() { service_->Destroy(); }

void GLES2ClientImpl::DidCreateContext(uint64_t encoded,
                                       uint32_t width,
                                       uint32_t height) {
  TRACE_EVENT0("compositor_app", "DidCreateContext");
  impl_ = reinterpret_cast<gpu::gles2::GLES2Implementation*>(encoded);
  if (!context_created_callback_.is_null())
    context_created_callback_.Run(gfx::Size(width, height));
}

gpu::gles2::GLES2Interface* GLES2ClientImpl::Interface() const {
  return impl_;
}

gpu::ContextSupport* GLES2ClientImpl::Support() const {
  return impl_;
}

void GLES2ClientImpl::ContextLost() { impl_ = NULL; }

}  // namespace examples
}  // namespace mojo
