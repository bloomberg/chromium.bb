// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/gpu_impl.h"

#include "components/view_manager/gles2/command_buffer_driver.h"
#include "components/view_manager/gles2/command_buffer_impl.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gles2 {

GpuImpl::GpuImpl(mojo::InterfaceRequest<Gpu> request,
                 const scoped_refptr<GpuState>& state)
    : binding_(this, request.Pass()), state_(state) {
}

GpuImpl::~GpuImpl() {
}

void GpuImpl::CreateOffscreenGLES2Context(
    mojo::InterfaceRequest<mojo::CommandBuffer> request) {
  new CommandBufferImpl(request.Pass(), mojo::ViewportParameterListenerPtr(),
                        state_->control_task_runner(),
                        state_->sync_point_manager(),
                        make_scoped_ptr(new CommandBufferDriver(
                            state_->share_group(), state_->mailbox_manager(),
                            state_->sync_point_manager())));
}

}  // namespace gles2
