// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_decoder_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/command_buffer_proxy.h"

namespace content {

RendererGpuVideoDecoderFactories::~RendererGpuVideoDecoderFactories() {}
RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    WebGraphicsContext3DCommandBufferImpl* context)
    : message_loop_(message_loop),
      gpu_channel_host_(gpu_channel_host) {
  if (message_loop_->BelongsToCurrentThread()) {
    AsyncGetContext(context, NULL);
    return;
  }
  // Threaded compositor requires us to wait for the context to be acquired.
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncGetContext,
      // Unretained to avoid ref/deref'ing |*this|, which is not yet stored in a
      // scoped_refptr.  Safe because the Wait() below keeps us alive until this
      // task completes.
      base::Unretained(this),
      // OK to pass raw because the pointee is only deleted on the compositor
      // thread, and only as the result of a PostTask from the render thread
      // which can only happen after this function returns, so our PostTask will
      // run first.
      context,
      &waiter));
  waiter.Wait();
}

void RendererGpuVideoDecoderFactories::AsyncGetContext(
    WebGraphicsContext3DCommandBufferImpl* context,
    base::WaitableEvent* waiter) {
  context_ = context->AsWeakPtr();
  if (context_) {
    if (context_->makeContextCurrent()) {
      // Called once per media player, but is a no-op after the first one in
      // each renderer.
      context_->insertEventMarkerEXT("GpuVDAContext3D");
    }
  }
  if (waiter)
    waiter->Signal();
}

media::VideoDecodeAccelerator*
RendererGpuVideoDecoderFactories::CreateVideoDecodeAccelerator(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  DCHECK(!message_loop_->BelongsToCurrentThread());
  media::VideoDecodeAccelerator* vda = NULL;
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator,
      this, profile, client, &vda, &waiter));
  waiter.Wait();
  return vda;
}

void RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client,
      media::VideoDecodeAccelerator** vda,
      base::WaitableEvent* waiter) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (context_ && context_->GetCommandBufferProxy()) {
    *vda = gpu_channel_host_->CreateVideoDecoder(
        context_->GetCommandBufferProxy()->GetRouteID(),
        profile, client);
  } else {
    *vda = NULL;
  }
  waiter->Signal();
}

bool RendererGpuVideoDecoderFactories::CreateTextures(
    int32 count, const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    uint32 texture_target) {
  DCHECK(!message_loop_->BelongsToCurrentThread());
  bool success = false;
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateTextures, this,
      count, size, texture_ids, texture_target, &success, &waiter));
  waiter.Wait();
  return success;
}

void RendererGpuVideoDecoderFactories::AsyncCreateTextures(
    int32 count, const gfx::Size& size, std::vector<uint32>* texture_ids,
    uint32 texture_target, bool* success, base::WaitableEvent* waiter) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(texture_target);
  if (!context_) {
    *success = false;
    waiter->Signal();
    return;
  }
  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  texture_ids->resize(count);
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
      gles2->TexImage2D(texture_target, 0, GL_RGBA, size.width(), size.height(),
                        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
  }
  // We need a glFlush here to guarantee the decoder (in the GPU process) can
  // use the texture ids we return here.  Since textures are expected to be
  // reused, this should not be unacceptably expensive.
  gles2->Flush();
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  *success = true;
  waiter->Signal();
}

void RendererGpuVideoDecoderFactories::DeleteTexture(uint32 texture_id) {
  DCHECK(!message_loop_->BelongsToCurrentThread());
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncDeleteTexture, this, texture_id));
}

void RendererGpuVideoDecoderFactories::AsyncDeleteTexture(uint32 texture_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_)
    return;
  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoDecoderFactories::ReadPixels(
    uint32 texture_id, uint32 texture_target, const gfx::Size& size,
    void* pixels) {
  base::WaitableEvent waiter(false, false);
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &RendererGpuVideoDecoderFactories::AsyncReadPixels, this,
        texture_id, texture_target, size, pixels, &waiter));
    waiter.Wait();
  } else {
    AsyncReadPixels(texture_id, texture_target, size, pixels, &waiter);
  }
}

void RendererGpuVideoDecoderFactories::AsyncReadPixels(
    uint32 texture_id, uint32 texture_target, const gfx::Size& size,
    void* pixels, base::WaitableEvent* waiter) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!context_)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();

  GLuint tmp_texture;
  gles2->GenTextures(1, &tmp_texture);
  gles2->BindTexture(texture_target, tmp_texture);
  gles2->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  context_->copyTextureCHROMIUM(
      texture_target, texture_id, tmp_texture, 0, GL_RGBA);

  GLuint fb;
  gles2->GenFramebuffers(1, &fb);
  gles2->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gles2->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target, tmp_texture, 0);
  gles2->PixelStorei(GL_PACK_ALIGNMENT, 4);
  gles2->ReadPixels(0, 0, size.width(), size.height(), GL_BGRA_EXT,
                    GL_UNSIGNED_BYTE, pixels);
  gles2->DeleteFramebuffers(1, &fb);
  gles2->DeleteTextures(1, &tmp_texture);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  waiter->Signal();
}

base::SharedMemory* RendererGpuVideoDecoderFactories::CreateSharedMemory(
    size_t size) {
  DCHECK_NE(MessageLoop::current(), ChildThread::current()->message_loop());
  base::SharedMemory* shm = NULL;
  base::WaitableEvent waiter(false, false);
  ChildThread::current()->message_loop()->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory, this,
      size, &shm, &waiter));
  waiter.Wait();
  return shm;
}

void RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory(
    size_t size, base::SharedMemory** shm, base::WaitableEvent* waiter) {
  DCHECK_EQ(MessageLoop::current(), ChildThread::current()->message_loop());
  *shm = ChildThread::current()->AllocateSharedMemory(size);
  waiter->Signal();
}

scoped_refptr<base::MessageLoopProxy>
RendererGpuVideoDecoderFactories::GetMessageLoop() {
  return message_loop_;
}

}  // namespace content
