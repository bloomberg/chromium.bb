// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
#define CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/filters/gpu_video_decoder.h"
#include "ui/gfx/size.h"

class GpuChannelHost;
class RendererGLContext;
namespace base {
class WaitableEvent;
}

// Glue code to expose functionality needed by media::GpuVideoDecoder to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The public methods of the class can be called from any thread, and are
// internally trampolined to the thread on which the class was constructed
// (de-facto, the renderer thread) if called from a different thread.
class CONTENT_EXPORT RendererGpuVideoDecoderFactories
    : public media::GpuVideoDecoder::Factories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for NULL before each
  // use.
  RendererGpuVideoDecoderFactories(GpuChannelHost* gpu_channel_host,
                                   base::WeakPtr<RendererGLContext> context);

  virtual media::VideoDecodeAccelerator* CreateVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Profile profile,
      media::VideoDecodeAccelerator::Client* client) OVERRIDE;

  virtual bool CreateTextures(int32 count, const gfx::Size& size,
                              std::vector<uint32>* texture_ids) OVERRIDE;

  virtual void DeleteTexture(uint32 texture_id) OVERRIDE;

  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<RendererGpuVideoDecoderFactories>;
  virtual ~RendererGpuVideoDecoderFactories();

 private:
  // Async versions of the public methods.  These all run on |message_loop_|
  // exclusively, and use output parameters instead of return values.  Finally,
  // each takes a WaitableEvent* param to signal completion (except for
  // DeleteTexture, which is fire-and-forget).
  void AsyncCreateVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Profile profile,
      media::VideoDecodeAccelerator::Client* client,
      media::VideoDecodeAccelerator** vda,
      base::WaitableEvent* waiter);
  void AsyncCreateTextures(
      int32 count, const gfx::Size& size, std::vector<uint32>* texture_ids,
      bool* success, base::WaitableEvent* waiter);
  void AsyncDeleteTexture(uint32 texture_id);
  void AsyncCreateSharedMemory(
      size_t size, base::SharedMemory** shm, base::WaitableEvent* waiter);

  MessageLoop* message_loop_;
  scoped_refptr<GpuChannelHost> gpu_channel_host_;
  base::WeakPtr<RendererGLContext> context_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(RendererGpuVideoDecoderFactories);
};

#endif  // CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
