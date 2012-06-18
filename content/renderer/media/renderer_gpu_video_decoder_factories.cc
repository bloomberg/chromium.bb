// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_decoder_factories.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/command_buffer_proxy.h"

RendererGpuVideoDecoderFactories::~RendererGpuVideoDecoderFactories() {}
RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories(
    GpuChannelHost* gpu_channel_host, MessageLoop* message_loop,
    const base::WeakPtr<WebGraphicsContext3DCommandBufferImpl>& context)
    : message_loop_(message_loop),
      gpu_channel_host_(gpu_channel_host),
      context_(context) {
  DCHECK(context_);
  context_->DetachFromThread();
  if (MessageLoop::current() == message_loop_) {
    AsyncGetContext(NULL);
    return;
  }
  // Threaded compositor requires us to wait for the context to be acquired.
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncGetContext,
      // Unretained to avoid ref/deref'ing |*this|, which is not yet stored in a
      // scoped_refptr.  Safe because the Wait() below keeps us alive until this
      // task completes.
      base::Unretained(this), &waiter));
  waiter.Wait();
}

void RendererGpuVideoDecoderFactories::AsyncGetContext(
    base::WaitableEvent* waiter) {
  if (context_)
    context_->makeContextCurrent();
  if (waiter)
    waiter->Signal();
}

media::VideoDecodeAccelerator*
RendererGpuVideoDecoderFactories::CreateVideoDecodeAccelerator(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  DCHECK_NE(MessageLoop::current(), message_loop_);
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
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  if (context_) {
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
  DCHECK_NE(MessageLoop::current(), message_loop_);
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
  DCHECK_EQ(MessageLoop::current(), message_loop_);
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
    gles2->TexParameterf(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameterf(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gles2->TexImage2D(texture_target, 0, GL_RGBA, size.width(), size.height(),
                      0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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
  DCHECK_NE(MessageLoop::current(), message_loop_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncDeleteTexture, this, texture_id));
}

void RendererGpuVideoDecoderFactories::AsyncDeleteTexture(uint32 texture_id) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  if (!context_)
    return;
  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
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
