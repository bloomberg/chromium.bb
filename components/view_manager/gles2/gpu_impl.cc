// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/gpu_impl.h"

#include "components/view_manager/gles2/command_buffer_driver.h"
#include "components/view_manager/gles2/command_buffer_impl.h"

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
                        state_,
                        make_scoped_ptr(new CommandBufferDriver(state_)));
}

}  // namespace gles2
