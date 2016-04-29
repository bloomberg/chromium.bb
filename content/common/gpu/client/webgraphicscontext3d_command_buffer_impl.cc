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
    gpu::GpuChannelHost* host,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    gfx::GpuPreference gpu_preference,
    bool automatic_flushes)
    : automatic_flushes_(automatic_flushes),
      attributes_(attributes),
      host_(host),
      surface_handle_(surface_handle),
      active_url_(active_url),
      gpu_preference_(gpu_preference),
      weak_ptr_factory_(this) {
  DCHECK(host);
  switch (attributes.context_type) {
    case gpu::gles2::CONTEXT_TYPE_OPENGLES2:
    case gpu::gles2::CONTEXT_TYPE_OPENGLES3:
      context_type_ = CONTEXT_TYPE_UNKNOWN;
    case gpu::gles2::CONTEXT_TYPE_WEBGL1:
    case gpu::gles2::CONTEXT_TYPE_WEBGL2:
      context_type_ = OFFSCREEN_CONTEXT_FOR_WEBGL;
  }
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (real_gl_)
    real_gl_->SetLostContextCallback(base::Closure());

  Destroy();
}

bool WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL(
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::CommandBufferProxyImpl* shared_command_buffer,
    scoped_refptr<gpu::gles2::ShareGroup> share_group) {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::MaybeInitializeGL");

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "125248 WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL"));

  if (!CreateContext(memory_limits, shared_command_buffer,
                     std::move(share_group))) {
    Destroy();

    initialize_failed_ = true;
    return false;
  }

  real_gl_->SetLostContextCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnContextLost,
                 // The callback is unset in the destructor.
                 base::Unretained(this)));

  real_gl_->TraceBeginCHROMIUM("WebGraphicsContext3D",
                               "CommandBufferContext");

  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeCommandBuffer(
    gpu::CommandBufferProxyImpl* shared_command_buffer) {
  if (!host_.get())
    return false;

  DCHECK(attributes_.buffer_preserved);
  std::vector<int32_t> serialized_attributes;
  attributes_.Serialize(&serialized_attributes);

  // Create a proxy to a command buffer in the GPU process.
  command_buffer_ = host_->CreateCommandBuffer(
      surface_handle_, gfx::Size(), shared_command_buffer,
      gpu::GpuChannelHost::kDefaultStreamId,
      gpu::GpuChannelHost::kDefaultStreamPriority, serialized_attributes,
      active_url_, gpu_preference_);

  if (!command_buffer_) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    UmaRecordContextInitFailed(context_type_);
    return false;
  }

  DVLOG_IF(1, gpu::error::IsError(command_buffer_->GetLastError()))
      << "Context dead on arrival. Last error: "
      << command_buffer_->GetLastError();
  // Initialize the command buffer.
  bool result = command_buffer_->Initialize();
  LOG_IF(ERROR, !result) << "CommandBufferProxy::Initialize failed.";
  if (!result)
    UmaRecordContextInitFailed(context_type_);
  return result;
}

bool WebGraphicsContext3DCommandBufferImpl::CreateContext(
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::CommandBufferProxyImpl* shared_command_buffer,
    scoped_refptr<gpu::gles2::ShareGroup> share_group) {
  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::CreateContext");
  DCHECK_EQ(!!shared_command_buffer, !!share_group);

  if (!InitializeCommandBuffer(shared_command_buffer)) {
    LOG(ERROR) << "Failed to initialize command buffer.";
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(memory_limits.command_buffer_size)) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
    return false;
  }

  if (!automatic_flushes_)
    gles2_helper_->SetAutomaticFlushes(false);
  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_ .reset(new gpu::TransferBuffer(gles2_helper_.get()));

  DCHECK(host_.get());

  const bool bind_generates_resource = attributes_.bind_generates_resource;
  const bool lose_context_when_out_of_memory =
      attributes_.lose_context_when_out_of_memory;
  const bool support_client_side_arrays = false;

  // Create the object exposing the OpenGL API.
  real_gl_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), std::move(share_group), transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, command_buffer_.get()));
  if (!real_gl_->Initialize(memory_limits.start_transfer_buffer_size,
                            memory_limits.min_transfer_buffer_size,
                            memory_limits.max_transfer_buffer_size,
                            memory_limits.mapped_memory_reclaim_limit)) {
    LOG(ERROR) << "Failed to initialize GLES2Implementation.";
    return false;
  }

  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeOnCurrentThread(
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::CommandBufferProxyImpl* shared_command_buffer,
    scoped_refptr<gpu::gles2::ShareGroup> share_group) {
  if (!MaybeInitializeGL(memory_limits, shared_command_buffer,
                         std::move(share_group))) {
    DLOG(ERROR) << "Failed to initialize context.";
    return false;
  }
  if (gpu::error::IsError(command_buffer_->GetLastError())) {
    LOG(ERROR) << "Context dead on arrival. Last error: "
               << command_buffer_->GetLastError();
    return false;
  }

  return true;
}

void WebGraphicsContext3DCommandBufferImpl::Destroy() {
  trace_gl_.reset();
  real_gl_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  real_gl_.reset();
  command_buffer_.reset();

  host_ = nullptr;
}

gpu::ContextSupport*
WebGraphicsContext3DCommandBufferImpl::GetContextSupport() {
  return real_gl_.get();
}

void WebGraphicsContext3DCommandBufferImpl::OnContextLost() {
  if (context_lost_callback_)
    context_lost_callback_->onContextLost();

  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  UmaRecordContextLost(context_type_, state.error, state.context_lost_reason);
}

}  // namespace content
