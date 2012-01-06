// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_gpu_video_decoder_factories.h"

#include "content/common/child_thread.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

RendererGpuVideoDecoderFactories::~RendererGpuVideoDecoderFactories() {}
RendererGpuVideoDecoderFactories::RendererGpuVideoDecoderFactories(
    GpuChannelHost* gpu_channel_host, base::WeakPtr<RendererGLContext> context)
    : gpu_channel_host_(gpu_channel_host),
      context_(context) {
}

media::VideoDecodeAccelerator*
RendererGpuVideoDecoderFactories::CreateVideoDecodeAccelerator(
    media::VideoDecodeAccelerator::Profile profile,
    media::VideoDecodeAccelerator::Client* client) {
  if (!context_)
    return NULL;
  return gpu_channel_host_->CreateVideoDecoder(
      context_->GetCommandBufferProxy()->route_id(), profile, client);
}

bool RendererGpuVideoDecoderFactories::CreateTextures(
    int32 count, const gfx::Size& size, std::vector<uint32>* texture_ids) {
  if (!context_)
    return false;
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
  return true;
}

bool RendererGpuVideoDecoderFactories::DeleteTexture(uint32 texture_id) {
  if (!context_)
    return false;
  gpu::gles2::GLES2Implementation* gles2 = context_->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  return true;
}

base::SharedMemory* RendererGpuVideoDecoderFactories::CreateSharedMemory(
    size_t size) {
  return ChildThread::current()->AllocateSharedMemory(size);
}
