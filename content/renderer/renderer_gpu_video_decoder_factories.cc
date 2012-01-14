// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_gpu_video_decoder_factories.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/child_thread.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

RendererGpuVideoDecoderFactories::~RendererGpuVideoDecoderFactories() {}
RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories(
    GpuChannelHost* gpu_channel_host, base::WeakPtr<RendererGLContext> context)
    : message_loop_(MessageLoop::current()),
      gpu_channel_host_(gpu_channel_host),
      context_(context) {
}

media::VideoDecodeAccelerator*
RendererGpuVideoDecoderFactories::CreateVideoDecodeAccelerator(
    media::VideoDecodeAccelerator::Profile profile,
    media::VideoDecodeAccelerator::Client* client) {
  media::VideoDecodeAccelerator* vda = NULL;
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator,
      this, profile, client, &vda, &waiter));
  waiter.Wait();
  return vda;
}

void RendererGpuVideoDecoderFactories::AsyncCreateVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Profile profile,
      media::VideoDecodeAccelerator::Client* client,
      media::VideoDecodeAccelerator** vda,
      base::WaitableEvent* waiter) {
  if (context_) {
    *vda = gpu_channel_host_->CreateVideoDecoder(
        context_->GetCommandBufferProxy()->route_id(), profile, client);
  } else {
    *vda = NULL;
  }
  waiter->Signal();
}

bool RendererGpuVideoDecoderFactories::CreateTextures(
    int32 count, const gfx::Size& size, std::vector<uint32>* texture_ids) {
  bool success = false;
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateTextures, this,
      count, size, texture_ids, &success, &waiter));
  waiter.Wait();
  return success;
}

void RendererGpuVideoDecoderFactories::AsyncCreateTextures(
    int32 count, const gfx::Size& size, std::vector<uint32>* texture_ids,
    bool* success, base::WaitableEvent* waiter) {
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
    gles2->BindTexture(GL_TEXTURE_2D, texture_id);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gles2->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(),
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
  base::SharedMemory* shm = NULL;
  base::WaitableEvent waiter(false, false);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory, this,
      size, &shm, &waiter));
  waiter.Wait();
  return shm;
}

void RendererGpuVideoDecoderFactories::AsyncCreateSharedMemory(
    size_t size, base::SharedMemory** shm, base::WaitableEvent* waiter) {
  *shm = ChildThread::current()->AllocateSharedMemory(size);
  waiter->Signal();
}
