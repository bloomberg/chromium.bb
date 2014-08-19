// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/gpu_impl.h"

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "ui/gl/gl_share_group.h"

namespace mojo {

GpuImpl::GpuImpl(
    const scoped_refptr<gfx::GLShareGroup>& share_group,
    const scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager)
    : share_group_(share_group), mailbox_manager_(mailbox_manager) {
}

GpuImpl::~GpuImpl() {
}

void GpuImpl::CreateOnscreenGLES2Context(
    uint64_t native_viewport_id,
    SizePtr size,
    InterfaceRequest<CommandBuffer> command_buffer_request) {
  gfx::AcceleratedWidget widget = bit_cast<gfx::AcceleratedWidget>(
      static_cast<uintptr_t>(native_viewport_id));
  BindToRequest(new CommandBufferImpl(widget,
                                      size.To<gfx::Size>(),
                                      share_group_.get(),
                                      mailbox_manager_.get()),
                &command_buffer_request);
}

void GpuImpl::CreateOffscreenGLES2Context(
    InterfaceRequest<CommandBuffer> command_buffer_request) {
  BindToRequest(
      new CommandBufferImpl(share_group_.get(), mailbox_manager_.get()),
      &command_buffer_request);
}

}  // namespace mojo
