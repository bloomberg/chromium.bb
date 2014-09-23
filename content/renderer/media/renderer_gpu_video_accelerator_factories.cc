// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"

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
      thread_safe_sender_(ChildThread::current()->thread_safe_sender()) {}

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

void RendererGpuVideoAcceleratorFactories::ReadPixels(
    uint32 texture_id,
    const gfx::Rect& visible_rect,
    const SkBitmap& pixels) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  GLHelper* gl_helper = GetGLHelper();
  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();

  if (!gl_helper || !context)
    return;

  // Copy texture from texture_id to tmp_texture as texture might be external
  // (GL_TEXTURE_EXTERNAL_OES)
  GLuint tmp_texture;
  tmp_texture = gl_helper->CreateTexture();
  context->copyTextureCHROMIUM(
      GL_TEXTURE_2D, texture_id, tmp_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE);

  unsigned char* pixel_data =
      static_cast<unsigned char*>(pixels.pixelRef()->pixels());

  if (gl_helper->IsReadbackConfigSupported(pixels.colorType())) {
    gl_helper->ReadbackTextureSync(
        tmp_texture, visible_rect, pixel_data, pixels.colorType());
  } else if (pixels.colorType() == kN32_SkColorType) {
    gl_helper->ReadbackTextureSync(
        tmp_texture, visible_rect, pixel_data, kRGBA_8888_SkColorType);

    int pixel_count = visible_rect.width() * visible_rect.height();
    uint32_t* pixels_ptr = static_cast<uint32_t*>(pixels.pixelRef()->pixels());
    for (int i = 0; i < pixel_count; ++i) {
        uint32_t r = pixels_ptr[i] & 0xFF;
        uint32_t g = (pixels_ptr[i] >> 8) & 0xFF;
        uint32_t b = (pixels_ptr[i] >> 16) & 0xFF;
        uint32_t a = (pixels_ptr[i] >> 24) & 0xFF;
        pixels_ptr[i] = (r << SK_R32_SHIFT) |
                        (g << SK_G32_SHIFT) |
                        (b << SK_B32_SHIFT) |
                        (a << SK_A32_SHIFT);
    }
  } else {
    NOTREACHED();
  }

  gl_helper->DeleteTexture(tmp_texture);
}

base::SharedMemory* RendererGpuVideoAcceleratorFactories::CreateSharedMemory(
    size_t size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return ChildThread::AllocateSharedMemory(size, thread_safe_sender_.get());
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererGpuVideoAcceleratorFactories::GetTaskRunner() {
  return task_runner_;
}

std::vector<media::VideoEncodeAccelerator::SupportedProfile>
RendererGpuVideoAcceleratorFactories::
    GetVideoEncodeAcceleratorSupportedProfiles() {
  return gpu_channel_host_->gpu_info()
      .video_encode_accelerator_supported_profiles;
}

}  // namespace content
