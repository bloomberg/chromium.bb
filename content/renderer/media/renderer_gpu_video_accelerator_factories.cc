// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "content/child/child_thread_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

// static
scoped_refptr<RendererGpuVideoAcceleratorFactories>
RendererGpuVideoAcceleratorFactories::Create(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider) {
  scoped_refptr<RendererGpuVideoAcceleratorFactories> factories =
      new RendererGpuVideoAcceleratorFactories(
          gpu_channel_host, task_runner, context_provider);
  // Post task from outside constructor, since AddRef()/Release() is unsafe from
  // within.
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RendererGpuVideoAcceleratorFactories::BindContext,
                 factories));
  return factories;
}

RendererGpuVideoAcceleratorFactories::RendererGpuVideoAcceleratorFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider)
    : task_runner_(task_runner),
      gpu_channel_host_(gpu_channel_host),
      context_provider_(context_provider),
      thread_safe_sender_(ChildThreadImpl::current()->thread_safe_sender()) {}

RendererGpuVideoAcceleratorFactories::~RendererGpuVideoAcceleratorFactories() {}

void RendererGpuVideoAcceleratorFactories::BindContext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!context_provider_->BindToCurrentThread())
    context_provider_ = NULL;
}

WebGraphicsContext3DCommandBufferImpl*
RendererGpuVideoAcceleratorFactories::GetContext3d() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!context_provider_.get())
    return NULL;
  if (context_provider_->IsContextLost()) {
    context_provider_->VerifyContexts();
    context_provider_ = NULL;
    gl_helper_.reset(NULL);
    return NULL;
  }
  return context_provider_->WebContext3D();
}

GLHelper* RendererGpuVideoAcceleratorFactories::GetGLHelper() {
  if (!GetContext3d())
    return NULL;

  if (gl_helper_.get() == NULL) {
    gl_helper_.reset(new GLHelper(GetContext3d()->GetImplementation(),
                                  GetContext3d()->GetContextSupport()));
  }

  return gl_helper_.get();
}

scoped_ptr<media::VideoDecodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (context && context->GetCommandBufferProxy()) {
    return gpu_channel_host_->CreateVideoDecoder(
        context->GetCommandBufferProxy()->GetRouteID());
  }

  return scoped_ptr<media::VideoDecodeAccelerator>();
}

scoped_ptr<media::VideoEncodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (context && context->GetCommandBufferProxy()) {
    return gpu_channel_host_->CreateVideoEncoder(
        context->GetCommandBufferProxy()->GetRouteID());
  }

  return scoped_ptr<media::VideoEncodeAccelerator>();
}

bool RendererGpuVideoAcceleratorFactories::CreateTextures(
    int32 count,
    const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32 texture_target) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(texture_target);

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return false;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  texture_ids->resize(count);
  texture_mailboxes->resize(count);
  gles2->GenTextures(count, &texture_ids->at(0));
  for (int i = 0; i < count; ++i) {
    gles2->ActiveTexture(GL_TEXTURE0);
    uint32 texture_id = texture_ids->at(i);
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

void RendererGpuVideoAcceleratorFactories::DeleteTexture(uint32 texture_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoAcceleratorFactories::WaitSyncPoint(uint32 sync_point) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->WaitSyncPointCHROMIUM(sync_point);

  // Callers expect the WaitSyncPoint to affect the next IPCs. Make sure to
  // flush the command buffers to ensure that.
  gles2->ShallowFlushCHROMIUM();
}

scoped_ptr<base::SharedMemory>
RendererGpuVideoAcceleratorFactories::CreateSharedMemory(size_t size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
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

std::vector<media::VideoEncodeAccelerator::SupportedProfile>
RendererGpuVideoAcceleratorFactories::
    GetVideoEncodeAcceleratorSupportedProfiles() {
  return GpuVideoEncodeAcceleratorHost::ConvertGpuToMediaProfiles(
      gpu_channel_host_->gpu_info()
          .video_encode_accelerator_supported_profiles);
}

}  // namespace content
