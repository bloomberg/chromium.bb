// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace content {

RendererGpuVideoAcceleratorFactories::~RendererGpuVideoAcceleratorFactories() {}
RendererGpuVideoAcceleratorFactories::RendererGpuVideoAcceleratorFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider)
    : message_loop_(
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy()),
      gpu_channel_host_(gpu_channel_host),
      context_provider_(context_provider),
      thread_safe_sender_(ChildThread::current()->thread_safe_sender()),
      aborted_waiter_(true, false),
      message_loop_async_waiter_(false, false) {
  // |context_provider_| is only required to support HW-accelerated decode.
  if (!context_provider_)
    return;

  if (message_loop_->BelongsToCurrentThread()) {
    AsyncBindContext();
    message_loop_async_waiter_.Reset();
    return;
  }
  // Wait for the context to be acquired.
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&RendererGpuVideoAcceleratorFactories::AsyncBindContext,
                 // Unretained to avoid ref/deref'ing |*this|, which is not yet
                 // stored in a scoped_refptr.  Safe because the Wait() below
                 // keeps us alive until this task completes.
                 base::Unretained(this)));
  message_loop_async_waiter_.Wait();
}

RendererGpuVideoAcceleratorFactories::RendererGpuVideoAcceleratorFactories()
    : aborted_waiter_(true, false),
      message_loop_async_waiter_(false, false) {}

WebGraphicsContext3DCommandBufferImpl*
RendererGpuVideoAcceleratorFactories::GetContext3d() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_provider_)
    return NULL;
  WebGraphicsContext3DCommandBufferImpl* context =
      context_provider_->Context3d();
  if (context->isContextLost()) {
    context_provider_->VerifyContexts();
    context_provider_ = NULL;
    return NULL;
  }
  return context;
}

void RendererGpuVideoAcceleratorFactories::AsyncBindContext() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_provider_->BindToCurrentThread())
    context_provider_ = NULL;
  message_loop_async_waiter_.Signal();
}

scoped_ptr<media::VideoDecodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncCreateVideoDecodeAccelerator(profile, client);
    message_loop_async_waiter_.Reset();
    return vda_.Pass();
  }
  // The VDA is returned in the vda_ member variable by the
  // AsyncCreateVideoDecodeAccelerator() function.
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&RendererGpuVideoAcceleratorFactories::
                                         AsyncCreateVideoDecodeAccelerator,
                                     this,
                                     profile,
                                     client));

  base::WaitableEvent* objects[] = {&aborted_waiter_,
                                    &message_loop_async_waiter_};
  if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0) {
    // If we are aborting and the VDA is created by the
    // AsyncCreateVideoDecodeAccelerator() function later we need to ensure
    // that it is destroyed on the same thread.
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RendererGpuVideoAcceleratorFactories::
                                           AsyncDestroyVideoDecodeAccelerator,
                                       this));
    return scoped_ptr<media::VideoDecodeAccelerator>();
  }
  return vda_.Pass();
}

scoped_ptr<media::VideoEncodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator(
    media::VideoEncodeAccelerator::Client* client) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  return gpu_channel_host_->CreateVideoEncoder(client);
}

void RendererGpuVideoAcceleratorFactories::AsyncCreateVideoDecodeAccelerator(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (context && context->GetCommandBufferProxy()) {
    vda_ = gpu_channel_host_->CreateVideoDecoder(
        context->GetCommandBufferProxy()->GetRouteID(), profile, client);
  }
  message_loop_async_waiter_.Signal();
}

uint32 RendererGpuVideoAcceleratorFactories::CreateTextures(
    int32 count,
    const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32 texture_target) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(texture_target);

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return 0;

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

  // We need a glFlush here to guarantee the decoder (in the GPU process) can
  // use the texture ids we return here.  Since textures are expected to be
  // reused, this should not be unacceptably expensive.
  gles2->Flush();
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));

  return gles2->InsertSyncPointCHROMIUM();
}

void RendererGpuVideoAcceleratorFactories::DeleteTexture(uint32 texture_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoAcceleratorFactories::WaitSyncPoint(uint32 sync_point) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->WaitSyncPointCHROMIUM(sync_point);

  // Callers expect the WaitSyncPoint to affect the next IPCs. Make sure to
  // flush the command buffers to ensure that.
  gles2->ShallowFlushCHROMIUM();
}

void RendererGpuVideoAcceleratorFactories::ReadPixels(uint32 texture_id,
                                                      const gfx::Size& size,
                                                      const SkBitmap& pixels) {
  // SkBitmaps use the SkPixelRef object to refcount the underlying pixels.
  // Multiple SkBitmaps can share a SkPixelRef instance. We use this to
  // ensure that the underlying pixels in the SkBitmap passed in remain valid
  // until the AsyncReadPixels() call completes.
  read_pixels_bitmap_.setPixelRef(pixels.pixelRef());

  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RendererGpuVideoAcceleratorFactories::AsyncReadPixels,
                   this,
                   texture_id,
                   size));
    base::WaitableEvent* objects[] = {&aborted_waiter_,
                                      &message_loop_async_waiter_};
    if (base::WaitableEvent::WaitMany(objects, arraysize(objects)) == 0)
      return;
  } else {
    AsyncReadPixels(texture_id, size);
    message_loop_async_waiter_.Reset();
  }
  read_pixels_bitmap_.setPixelRef(NULL);
}

void RendererGpuVideoAcceleratorFactories::AsyncReadPixels(
    uint32 texture_id,
    const gfx::Size& size) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context) {
    message_loop_async_waiter_.Signal();
    return;
  }

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();

  GLuint tmp_texture;
  gles2->GenTextures(1, &tmp_texture);
  gles2->BindTexture(GL_TEXTURE_2D, tmp_texture);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  context->copyTextureCHROMIUM(
      GL_TEXTURE_2D, texture_id, tmp_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE);

  GLuint fb;
  gles2->GenFramebuffers(1, &fb);
  gles2->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gles2->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_texture, 0);
  gles2->PixelStorei(GL_PACK_ALIGNMENT, 4);
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
  GLenum skia_format = GL_BGRA_EXT;
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
  GLenum skia_format = GL_RGBA;
#else
#error Unexpected Skia ARGB_8888 layout!
#endif
  gles2->ReadPixels(0,
                    0,
                    size.width(),
                    size.height(),
                    skia_format,
                    GL_UNSIGNED_BYTE,
                    read_pixels_bitmap_.pixelRef()->pixels());
  gles2->DeleteFramebuffers(1, &fb);
  gles2->DeleteTextures(1, &tmp_texture);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  message_loop_async_waiter_.Signal();
}

base::SharedMemory* RendererGpuVideoAcceleratorFactories::CreateSharedMemory(
    size_t size) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return ChildThread::AllocateSharedMemory(size, thread_safe_sender_.get());
}

scoped_refptr<base::MessageLoopProxy>
RendererGpuVideoAcceleratorFactories::GetMessageLoop() {
  return message_loop_;
}

void RendererGpuVideoAcceleratorFactories::Abort() { aborted_waiter_.Signal(); }

bool RendererGpuVideoAcceleratorFactories::IsAborted() {
  return aborted_waiter_.IsSignaled();
}

scoped_refptr<RendererGpuVideoAcceleratorFactories>
RendererGpuVideoAcceleratorFactories::Clone() {
  scoped_refptr<RendererGpuVideoAcceleratorFactories> factories =
      new RendererGpuVideoAcceleratorFactories();
  factories->message_loop_ = message_loop_;
  factories->gpu_channel_host_ = gpu_channel_host_;
  factories->context_provider_ = context_provider_;
  factories->thread_safe_sender_ = thread_safe_sender_;
  return factories;
}

void
RendererGpuVideoAcceleratorFactories::AsyncDestroyVideoDecodeAccelerator() {
  // OK to release because Destroy() will delete the VDA instance.
  if (vda_)
    vda_.release()->Destroy();
}

}  // namespace content
