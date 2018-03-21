// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gl_in_process_context.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_feature_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/surface_texture.h"
#endif

namespace gpu {

namespace {

class GLInProcessContextImpl
    : public GLInProcessContext,
      public base::SupportsWeakPtr<GLInProcessContextImpl> {
 public:
  GLInProcessContextImpl();
  ~GLInProcessContextImpl() override;

  // GLInProcessContext implementation:
  gpu::ContextResult Initialize(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
      scoped_refptr<gl::GLSurface> surface,
      bool is_offscreen,
      SurfaceHandle window,
      GLInProcessContext* share_context,
      const gpu::ContextCreationAttribs& attribs,
      const SharedMemoryLimits& mem_limits,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      ImageFactory* image_factory,
      GpuChannelManagerDelegate* gpu_channel_manager_delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  const gpu::Capabilities& GetCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  gles2::GLES2Implementation* GetImplementation() override;
  void SetSwapBuffersCompletionCallback(
      const gpu::InProcessCommandBuffer::SwapBuffersCompletionCallback&
          callback) override;
  void SetUpdateVSyncParametersCallback(
      const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
          callback) override;
  void SetPresentationCallback(
      const gpu::InProcessCommandBuffer::PresentationCallback& callback)
      override;
  void SetLock(base::Lock* lock) override;
  gpu::ServiceTransferCache* GetTransferCacheForTest() const override;

 private:
  void OnSignalSyncPoint(const base::Closure& callback);

  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
  std::unique_ptr<InProcessCommandBuffer> command_buffer_;

  DISALLOW_COPY_AND_ASSIGN(GLInProcessContextImpl);
};

GLInProcessContextImpl::GLInProcessContextImpl() = default;

GLInProcessContextImpl::~GLInProcessContextImpl() {
  if (gles2_implementation_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    gles2_implementation_.reset();
  }

  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();
}

const Capabilities& GLInProcessContextImpl::GetCapabilities() const {
  return command_buffer_->GetCapabilities();
}

const GpuFeatureInfo& GLInProcessContextImpl::GetGpuFeatureInfo() const {
  return command_buffer_->GetGpuFeatureInfo();
}

gles2::GLES2Implementation* GLInProcessContextImpl::GetImplementation() {
  return gles2_implementation_.get();
}

void GLInProcessContextImpl::SetSwapBuffersCompletionCallback(
    const gpu::InProcessCommandBuffer::SwapBuffersCompletionCallback&
        callback) {
  command_buffer_->SetSwapBuffersCompletionCallback(callback);
}

void GLInProcessContextImpl::SetUpdateVSyncParametersCallback(
    const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
        callback) {
  command_buffer_->SetUpdateVSyncParametersCallback(callback);
}

void GLInProcessContextImpl::SetPresentationCallback(
    const gpu::InProcessCommandBuffer::PresentationCallback& callback) {
  command_buffer_->SetPresentationCallback(callback);
}

void GLInProcessContextImpl::SetLock(base::Lock* lock) {
  NOTREACHED();
}

gpu::ServiceTransferCache* GLInProcessContextImpl::GetTransferCacheForTest()
    const {
  return command_buffer_->GetTransferCacheForTest();
}

gpu::ContextResult GLInProcessContextImpl::Initialize(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    SurfaceHandle window,
    GLInProcessContext* share_context,
    const ContextCreationAttribs& attribs,
    const SharedMemoryLimits& mem_limits,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // If a surface is provided, we are running in a webview and should not have
  // a task runner. We must have a task runner in all other cases.
  DCHECK_EQ(!!surface, !task_runner);
  if (surface) {
    DCHECK_EQ(surface->IsOffscreen(), is_offscreen);
    DCHECK_EQ(kNullSurfaceHandle, window);
  }
  DCHECK_GE(attribs.offscreen_framebuffer_size.width(), 0);
  DCHECK_GE(attribs.offscreen_framebuffer_size.height(), 0);

  command_buffer_ = std::make_unique<InProcessCommandBuffer>(service);

  scoped_refptr<gles2::ShareGroup> share_group;
  InProcessCommandBuffer* share_command_buffer = nullptr;
  if (share_context) {
    GLInProcessContextImpl* impl =
        static_cast<GLInProcessContextImpl*>(share_context);
    share_group = impl->gles2_implementation_->share_group();
    share_command_buffer = impl->command_buffer_.get();
    DCHECK(share_group);
    DCHECK(share_command_buffer);
  }

  auto result = command_buffer_->Initialize(
      surface, is_offscreen, window, attribs, share_command_buffer,
      gpu_memory_buffer_manager, image_factory, gpu_channel_manager_delegate,
      std::move(task_runner));
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return result;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ =
      std::make_unique<gles2::GLES2CmdHelper>(command_buffer_.get());
  result = gles2_helper_->Initialize(mem_limits.command_buffer_size);
  if (result != gpu::ContextResult::kSuccess) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper";
    return result;
  }

  // Create a transfer buffer.
  transfer_buffer_ = std::make_unique<TransferBuffer>(gles2_helper_.get());

  // Check for consistency.
  DCHECK(!attribs.bind_generates_resource);
  const bool bind_generates_resource = false;
  const bool support_client_side_arrays = false;

  // Create the object exposing the OpenGL API.
  gles2_implementation_ = std::make_unique<gles2::GLES2Implementation>(
      gles2_helper_.get(), share_group.get(), transfer_buffer_.get(),
      bind_generates_resource, attribs.lose_context_when_out_of_memory,
      support_client_side_arrays, command_buffer_.get());

  result = gles2_implementation_->Initialize(mem_limits);
  return result;
}

}  // anonymous namespace

// static
std::unique_ptr<GLInProcessContext> GLInProcessContext::CreateWithoutInit() {
  return std::make_unique<GLInProcessContextImpl>();
}

}  // namespace gpu
