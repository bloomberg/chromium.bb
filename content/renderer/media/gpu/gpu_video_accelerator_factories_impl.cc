// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/unguessable_token.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/ipc/client/gpu_video_decode_accelerator_host.h"
#include "media/gpu/ipc/client/gpu_video_encode_accelerator_host.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

namespace {

// This enum values match ContextProviderPhase in histograms.xml
enum ContextProviderPhase {
  CONTEXT_PROVIDER_ACQUIRED = 0,
  CONTEXT_PROVIDER_RELEASED = 1,
  CONTEXT_PROVIDER_RELEASED_MAX_VALUE = CONTEXT_PROVIDER_RELEASED,
};

void RecordContextProviderPhaseUmaEnum(const ContextProviderPhase phase) {
  UMA_HISTOGRAM_ENUMERATION("Media.GPU.HasEverLostContext", phase,
                            CONTEXT_PROVIDER_RELEASED_MAX_VALUE + 1);
}

}  // namespace

// static
std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>
GpuVideoAcceleratorFactoriesImpl::Create(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ui::ContextProviderCommandBuffer>& context_provider,
    bool enable_gpu_memory_buffer_video_frames,
    const viz::BufferToTextureTargetMap& image_texture_targets,
    bool enable_video_accelerator,
    media::mojom::VideoEncodeAcceleratorProviderPtrInfo unbound_vea_provider) {
  RecordContextProviderPhaseUmaEnum(
      ContextProviderPhase::CONTEXT_PROVIDER_ACQUIRED);
  return base::WrapUnique(new GpuVideoAcceleratorFactoriesImpl(
      std::move(gpu_channel_host), main_thread_task_runner, task_runner,
      context_provider, enable_gpu_memory_buffer_video_frames,
      image_texture_targets, enable_video_accelerator,
      std::move(unbound_vea_provider)));
}

GpuVideoAcceleratorFactoriesImpl::GpuVideoAcceleratorFactoriesImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ui::ContextProviderCommandBuffer>& context_provider,
    bool enable_gpu_memory_buffer_video_frames,
    const viz::BufferToTextureTargetMap& image_texture_targets,
    bool enable_video_accelerator,
    media::mojom::VideoEncodeAcceleratorProviderPtrInfo unbound_vea_provider)
    : main_thread_task_runner_(main_thread_task_runner),
      task_runner_(task_runner),
      gpu_channel_host_(std::move(gpu_channel_host)),
      context_provider_refptr_(context_provider),
      context_provider_(context_provider.get()),
      enable_gpu_memory_buffer_video_frames_(
          enable_gpu_memory_buffer_video_frames),
      image_texture_targets_(image_texture_targets),
      video_accelerator_enabled_(enable_video_accelerator),
      gpu_memory_buffer_manager_(
          RenderThreadImpl::current()->GetGpuMemoryBufferManager()),
      thread_safe_sender_(ChildThreadImpl::current()->thread_safe_sender()) {
  DCHECK(main_thread_task_runner_);
  DCHECK(gpu_channel_host_);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::
                         BindVideoEncodeAcceleratorProviderOnTaskRunner,
                     base::Unretained(this),
                     base::Passed(&unbound_vea_provider)));
}

GpuVideoAcceleratorFactoriesImpl::~GpuVideoAcceleratorFactoriesImpl() {}

bool GpuVideoAcceleratorFactoriesImpl::CheckContextLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (context_provider_) {
    bool release_context_provider = false;
    {
      viz::ContextProvider::ScopedContextLock lock(context_provider_);
      if (lock.ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
        context_provider_ = nullptr;
        release_context_provider = true;
      }
    }
    if (release_context_provider) {
      // Drop the reference on the main thread.
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GpuVideoAcceleratorFactoriesImpl::ReleaseContextProvider,
              base::Unretained(this)));
    }
  }
  return !context_provider_;
}

bool GpuVideoAcceleratorFactoriesImpl::IsGpuVideoAcceleratorEnabled() {
  return video_accelerator_enabled_;
}

base::UnguessableToken GpuVideoAcceleratorFactoriesImpl::GetChannelToken() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return base::UnguessableToken();

  if (channel_token_.is_empty()) {
    context_provider_->GetCommandBufferProxy()->channel()->Send(
        new GpuCommandBufferMsg_GetChannelToken(&channel_token_));
  }

  return channel_token_;
}

int32_t GpuVideoAcceleratorFactoriesImpl::GetCommandBufferRouteId() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return 0;
  return context_provider_->GetCommandBufferProxy()->route_id();
}

std::unique_ptr<media::VideoDecodeAccelerator>
GpuVideoAcceleratorFactoriesImpl::CreateVideoDecodeAccelerator() {
  DCHECK(video_accelerator_enabled_);
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return nullptr;

  return std::unique_ptr<media::VideoDecodeAccelerator>(
      new media::GpuVideoDecodeAcceleratorHost(
          context_provider_->GetCommandBufferProxy()));
}

std::unique_ptr<media::VideoEncodeAccelerator>
GpuVideoAcceleratorFactoriesImpl::CreateVideoEncodeAccelerator() {
  DCHECK(video_accelerator_enabled_);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(vea_provider_.is_bound());
  if (CheckContextLost())
    return nullptr;

  if (base::FeatureList::IsEnabled(features::kMojoVideoEncodeAccelerator)) {
    media::mojom::VideoEncodeAcceleratorPtr vea;
    vea_provider_->CreateVideoEncodeAccelerator(mojo::MakeRequest(&vea));

    if (vea) {
      return std::unique_ptr<media::VideoEncodeAccelerator>(
          new media::MojoVideoEncodeAccelerator(
              std::move(vea),
              context_provider_->GetCommandBufferProxy()
                  ->channel()
                  ->gpu_info()
                  .video_encode_accelerator_supported_profiles));
    }
  }

  return std::unique_ptr<media::VideoEncodeAccelerator>(
      new media::GpuVideoEncodeAcceleratorHost(
          context_provider_->GetCommandBufferProxy()));
}

bool GpuVideoAcceleratorFactoriesImpl::CreateTextures(
    int32_t count,
    const gfx::Size& size,
    std::vector<uint32_t>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32_t texture_target) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(texture_target);

  if (CheckContextLost())
    return false;
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
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
      gles2->TexImage2D(texture_target, 0, GL_RGBA, size.width(), size.height(),
                        0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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

void GpuVideoAcceleratorFactoriesImpl::DeleteTexture(uint32_t texture_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return;

  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

gpu::SyncToken GpuVideoAcceleratorFactoriesImpl::CreateSyncToken() {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gl = lock.ContextGL();
  gpu::SyncToken sync_token;
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  return sync_token;
}

void GpuVideoAcceleratorFactoriesImpl::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return;

  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Callers expect the WaitSyncToken to affect the next IPCs. Make sure to
  // flush the command buffers to ensure that.
  gles2->ShallowFlushCHROMIUM();
}

void GpuVideoAcceleratorFactoriesImpl::ShallowFlushCHROMIUM() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return;

  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  gpu::gles2::GLES2Interface* gles2 = lock.ContextGL();
  gles2->ShallowFlushCHROMIUM();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuVideoAcceleratorFactoriesImpl::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          size, format, usage, gpu::kNullSurfaceHandle);
  return buffer;
}
bool GpuVideoAcceleratorFactoriesImpl::ShouldUseGpuMemoryBuffersForVideoFrames()
    const {
  return enable_gpu_memory_buffer_video_frames_;
}

unsigned GpuVideoAcceleratorFactoriesImpl::ImageTextureTarget(
    gfx::BufferFormat format) {
  auto found = image_texture_targets_.find(viz::BufferToTextureTargetKey(
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE, format));
  DCHECK(found != image_texture_targets_.end());
  return found->second;
}

media::GpuVideoAcceleratorFactories::OutputFormat
GpuVideoAcceleratorFactoriesImpl::VideoFrameOutputFormat() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (CheckContextLost())
    return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  auto capabilities = context_provider_->ContextCapabilities();
  if (capabilities.image_ycbcr_420v)
    return media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB;
  if (capabilities.image_ycbcr_422)
    return media::GpuVideoAcceleratorFactories::OutputFormat::UYVY;
  if (capabilities.texture_rg)
    return media::GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB;
  return media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED;
}

namespace {
class ScopedGLContextLockImpl
    : public media::GpuVideoAcceleratorFactories::ScopedGLContextLock {
 public:
  ScopedGLContextLockImpl(viz::ContextProvider* context_provider)
      : lock_(context_provider) {}
  gpu::gles2::GLES2Interface* ContextGL() override { return lock_.ContextGL(); }

 private:
  viz::ContextProvider::ScopedContextLock lock_;
};
}  // namespace

std::unique_ptr<media::GpuVideoAcceleratorFactories::ScopedGLContextLock>
GpuVideoAcceleratorFactoriesImpl::GetGLContextLock() {
  if (CheckContextLost())
    return nullptr;
  return std::make_unique<ScopedGLContextLockImpl>(context_provider_);
}

std::unique_ptr<base::SharedMemory>
GpuVideoAcceleratorFactoriesImpl::CreateSharedMemory(size_t size) {
  std::unique_ptr<base::SharedMemory> mem(
      ChildThreadImpl::AllocateSharedMemory(size));
  if (mem && !mem->Map(size))
    return nullptr;
  return mem;
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuVideoAcceleratorFactoriesImpl::GetTaskRunner() {
  return task_runner_;
}

media::VideoDecodeAccelerator::Capabilities
GpuVideoAcceleratorFactoriesImpl::GetVideoDecodeAcceleratorCapabilities() {
  return media::GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
      gpu_channel_host_->gpu_info().video_decode_accelerator_capabilities);
}

media::VideoEncodeAccelerator::SupportedProfiles
GpuVideoAcceleratorFactoriesImpl::GetVideoEncodeAcceleratorSupportedProfiles() {
  return media::GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
      gpu_channel_host_->gpu_info()
          .video_encode_accelerator_supported_profiles);
}

viz::ContextProvider*
GpuVideoAcceleratorFactoriesImpl::GetMediaContextProvider() {
  return CheckContextLost() ? nullptr : context_provider_;
}

void GpuVideoAcceleratorFactoriesImpl::ReleaseContextProvider() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  RecordContextProviderPhaseUmaEnum(
      ContextProviderPhase::CONTEXT_PROVIDER_RELEASED);
  context_provider_refptr_ = nullptr;
}

scoped_refptr<ui::ContextProviderCommandBuffer>
GpuVideoAcceleratorFactoriesImpl::ContextProviderMainThread() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return context_provider_refptr_;
}

void GpuVideoAcceleratorFactoriesImpl::
    BindVideoEncodeAcceleratorProviderOnTaskRunner(
        media::mojom::VideoEncodeAcceleratorProviderPtrInfo
            unbound_vea_provider) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!vea_provider_.is_bound());
  vea_provider_.Bind(std::move(unbound_vea_provider));
}

}  // namespace content
