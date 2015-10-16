// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gpu_impl.h"

#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_impl.h"
#include "components/mus/gles2/command_buffer_type_conversions.h"

namespace mus {

GpuImpl::GpuImpl(mojo::InterfaceRequest<Gpu> request,
                 const scoped_refptr<GpuState>& state)
    : binding_(this, request.Pass()), state_(state) {
}

GpuImpl::~GpuImpl() {}

void GpuImpl::CreateOffscreenGLES2Context(
    mojo::InterfaceRequest<mojom::CommandBuffer> request) {
  new CommandBufferImpl(request.Pass(), state_,
                        make_scoped_ptr(new CommandBufferDriver(state_)));
}

void GpuImpl::GetGpuInfo(const GetGpuInfoCallback& callback) {
  callback.Run(mojom::GpuInfo::From<gpu::GPUInfo>(state_->gpu_info()));
}

}  // namespace mus
