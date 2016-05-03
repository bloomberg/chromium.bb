// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

#include <algorithm>
#include <map>
#include <memory>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/trace_event/trace_event.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace content {

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    gpu::SurfaceHandle surface_handle,
    const GURL& active_url,
    scoped_refptr<gpu::GpuChannelHost> host,
    gfx::GpuPreference gpu_preference,
    bool automatic_flushes)
    : automatic_flushes_(automatic_flushes),
      surface_handle_(surface_handle),
      active_url_(active_url),
      gpu_preference_(gpu_preference),
      host_(std::move(host)) {
  DCHECK(host_);
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {}

bool WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL(
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::CommandBufferProxyImpl* shared_command_buffer,
    scoped_refptr<gpu::gles2::ShareGroup> share_group,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    command_buffer_metrics::ContextType context_type) {
  DCHECK_EQ(!!shared_command_buffer, !!share_group);
  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::MaybeInitializeGL");

  DCHECK(attributes.buffer_preserved);
  std::vector<int32_t> serialized_attributes;
  attributes.Serialize(&serialized_attributes);

  // Create a proxy to a command buffer in the GPU process.
  command_buffer_ = host_->CreateCommandBuffer(
      surface_handle_, gfx::Size(), shared_command_buffer,
      gpu::GpuChannelHost::kDefaultStreamId,
      gpu::GpuChannelHost::kDefaultStreamPriority, serialized_attributes,
      active_url_, gpu_preference_);

  if (!command_buffer_) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    command_buffer_metrics::UmaRecordContextInitFailed(context_type);
    return false;
  }

  // The GLES2 helper writes the command buffer protocol.
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  gles2_helper_->SetAutomaticFlushes(automatic_flushes_);
  if (!gles2_helper_->Initialize(memory_limits.command_buffer_size)) {
    DLOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
    return false;
  }

  // The transfer buffer is used to copy resources between the client
  // process and the GPU process.
  transfer_buffer_ .reset(new gpu::TransferBuffer(gles2_helper_.get()));

  const bool bind_generates_resource = attributes.bind_generates_resource;
  const bool lose_context_when_out_of_memory =
      attributes.lose_context_when_out_of_memory;
  const bool support_client_side_arrays = false;

  // The GLES2Implementation exposes the OpenGLES2 API, as well as the
  // gpu::ContextSupport interface.
  real_gl_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), std::move(share_group), transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, command_buffer_.get()));
  if (!real_gl_->Initialize(memory_limits.start_transfer_buffer_size,
                            memory_limits.min_transfer_buffer_size,
                            memory_limits.max_transfer_buffer_size,
                            memory_limits.mapped_memory_reclaim_limit)) {
    DLOG(ERROR) << "Failed to initialize GLES2Implementation.";
    return false;
  }

  real_gl_->TraceBeginCHROMIUM("WebGraphicsContext3D", "CommandBufferContext");
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeOnCurrentThread(
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::CommandBufferProxyImpl* shared_command_buffer,
    scoped_refptr<gpu::gles2::ShareGroup> share_group,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    command_buffer_metrics::ContextType context_type) {
  if (!MaybeInitializeGL(memory_limits, shared_command_buffer,
                         std::move(share_group), attributes, context_type))
    return false;
  if (gpu::error::IsError(command_buffer_->GetLastError())) {
    DLOG(ERROR) << "Context dead on arrival. Last error: "
                << command_buffer_->GetLastError();
    return false;
  }

  return true;
}

}  // namespace content
