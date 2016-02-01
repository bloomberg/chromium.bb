// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "cc/output/context_provider.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_thread_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/media/gpu_video_accelerator_util.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

// static
scoped_ptr<RendererGpuVideoAcceleratorFactories>
RendererGpuVideoAcceleratorFactories::Create(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    bool enable_gpu_memory_buffer_video_frames,
    std::vector<unsigned> image_texture_targets,
    bool enable_video_accelerator) {
  return make_scoped_ptr(new RendererGpuVideoAcceleratorFactories(
      gpu_channel_host, main_thread_task_runner, task_runner, context_provider,
      enable_gpu_memory_buffer_video_frames, image_texture_targets,
      enable_video_accelerator));
}

RendererGpuVideoAcceleratorFactories::RendererGpuVideoAcceleratorFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    bool enable_gpu_memory_buffer_video_frames,
    std::vector<unsigned> image_texture_targets,
    bool enable_video_accelerator)
    : main_thread_task_runner_(main_thread_task_runner),
      task_runner_(task_runner),
      gpu_channel_host_(gpu_channel_host),
      context_provider_refptr_(context_provider),
      context_provider_(context_provider.get()),
      enable_gpu_memory_buffer_video_frames_(
          enable_gpu_memory_buffer_video_frames),
      image_texture_targets_(image_texture_targets),
      video_accelerator_enabled_(enable_video_accelerator),
      gpu_memory_buffer_manager_(ChildThreadImpl::current()
                                     ->gpu_memory_buffer_manager()),
      thread_safe_sender_(ChildThreadImpl::current()->thread_safe_sender()) {
  DCHECK(main_thread_task_runner_);
  DCHECK(gpu_channel_host_);
}

RendererGpuVideoAcceleratorFactories::~RendererGpuVideoAcceleratorFactories() {}

bool RendererGpuVideoAcceleratorFactories::CheckContextLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (context_provider_) {
    cc::ContextProvider::ScopedContextLock lock(context_provider_);
    if (lock.ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      context_provider_ = nullptr;
      // Drop the reference on the main thread.
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &RendererGpuVideoAcceleratorFactories::ReleaseContextProvider,
              base::Unretained(this)));
    }
  }
  return !context_provider_;
}

bool RendererGpuVideoAcceleratorFactories::IsGpuVideoAcceleratorEnabled() {
  return video_accelerator_enabled_;
}

scoped_ptr<media::VideoDecodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  DCHECK(video_accelerator_enabled_);
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return nullptr;
  return context_provider_->GetCommandBufferProxy()->CreateVideoDecoder();
}

scoped_ptr<media::VideoEncodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  DCHECK(video_accelerator_enabled_);
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return nullptr;
  return context_provider_->GetCommandBufferProxy()->CreateVideoEncoder();
}

bool RendererGpuVideoAcceleratorFactories::CreateTextures(
    int32_t count,
    const gfx::Size& size,
    std::vector<uint32_t>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32_t texture_target) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(texture_target);

  if (CheckContextLost())
    return false;
  cc::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  texture_ids->resize(count);
  texture_mailboxes->resize(count);
  gles2->GenTextures(count, &texture_ids->at(0));
  for (int i = 0; i < count; ++i) {
    gles2->ActiveTexture(GL_TEXTURE0);
    uint32_t texture_id = texture_ids->at(i);
    gles2->BindTexture(texture_target, texture_id);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (texture_target == GL_TEXTURE_2D) {
      gles2->TexImage2D(texture_target,
                        0,
                        GL_RGBA,
                        size.width(),
                        size.height(),
                        0,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        NULL);
    }
    gles2->GenMailboxCHROMIUM(texture_mailboxes->at(i).name);
    gles2->ProduceTextureCHROMIUM(texture_target,
                                  texture_mailboxes->at(i).name);
  }

  // We need ShallowFlushCHROMIUM() here to order the command buffer commands
  // with respect to IPC to the GPU process, to guarantee that the decoder in
  // the GPU process can use these textures as soon as it receives IPC
  // notification of them.
  gles2->ShallowFlushCHROMIUM();
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  return true;
}

void RendererGpuVideoAcceleratorFactories::DeleteTexture(uint32_t texture_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return;

  cc::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoAcceleratorFactories::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return;

  cc::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Callers expect the WaitSyncToken to affect the next IPCs. Make sure to
  // flush the command buffers to ensure that.
  gles2->ShallowFlushCHROMIUM();
}

scoped_ptr<gfx::GpuMemoryBuffer>
RendererGpuVideoAcceleratorFactories::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  scoped_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu_memory_buffer_manager_->AllocateGpuMemoryBuffer(size, format, usage);
  return buffer;
}
bool RendererGpuVideoAcceleratorFactories::
    ShouldUseGpuMemoryBuffersForVideoFrames() const {
  return enable_gpu_memory_buffer_video_frames_;
}

unsigned RendererGpuVideoAcceleratorFactories::ImageTextureTarget(
    gfx::BufferFormat format) {
  return image_texture_targets_[static_cast<int>(format)];
}

media::VideoPixelFormat
RendererGpuVideoAcceleratorFactories::VideoFrameOutputFormat() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return media::PIXEL_FORMAT_UNKNOWN;
  cc::ContextProvider::ScopedContextLock lock(context_provider_);
  auto capabilities = context_provider_->ContextCapabilities();
  if (capabilities.gpu.image_ycbcr_422)
    return media::PIXEL_FORMAT_UYVY;
  if (capabilities.gpu.texture_rg)
    return media::PIXEL_FORMAT_I420;
  return media::PIXEL_FORMAT_UNKNOWN;
}

namespace {
class ScopedGLContextLockImpl
    : public media::GpuVideoAcceleratorFactories::ScopedGLContextLock {
 public:
  ScopedGLContextLockImpl(cc::ContextProvider* context_provider)
      : lock_(context_provider) {}
  gpu::gles2::GLES2Interface* ContextGL() override { return lock_.ContextGL(); }

 private:
  cc::ContextProvider::ScopedContextLock lock_;
};
}  // namespace

scoped_ptr<media::GpuVideoAcceleratorFactories::ScopedGLContextLock>
RendererGpuVideoAcceleratorFactories::GetGLContextLock() {
  if (CheckContextLost())
    return nullptr;
  return make_scoped_ptr(new ScopedGLContextLockImpl(context_provider_));
}

scoped_ptr<base::SharedMemory>
RendererGpuVideoAcceleratorFactories::CreateSharedMemory(size_t size) {
  scoped_ptr<base::SharedMemory> mem(
      ChildThreadImpl::AllocateSharedMemory(size, thread_safe_sender_.get()));
  if (mem && !mem->Map(size))
    return nullptr;
  return mem;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererGpuVideoAcceleratorFactories::GetTaskRunner() {
  return task_runner_;
}

media::VideoDecodeAccelerator::Capabilities
RendererGpuVideoAcceleratorFactories::GetVideoDecodeAcceleratorCapabilities() {
  return GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
      gpu_channel_host_->gpu_info().video_decode_accelerator_capabilities);
}

media::VideoEncodeAccelerator::SupportedProfiles
RendererGpuVideoAcceleratorFactories::
    GetVideoEncodeAcceleratorSupportedProfiles() {
  return GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
      gpu_channel_host_->gpu_info()
          .video_encode_accelerator_supported_profiles);
}

void RendererGpuVideoAcceleratorFactories::ReleaseContextProvider() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  context_provider_refptr_ = nullptr;
}

scoped_refptr<ContextProviderCommandBuffer>
RendererGpuVideoAcceleratorFactories::ContextProviderMainThread() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return context_provider_refptr_;
}

}  // namespace content
