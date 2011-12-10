// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
#define CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/filters/gpu_video_decoder.h"
#include "ui/gfx/size.h"

class GpuChannelHost;
class RendererGLContext;

// Glue code to expose functionality needed by media::GpuVideoDecoder to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
class RendererGpuVideoDecoderFactories
    : public media::GpuVideoDecoder::Factories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for NULL before each
  // use.
  RendererGpuVideoDecoderFactories(GpuChannelHost* gpu_channel_host,
                                   base::WeakPtr<RendererGLContext> context);

  virtual ~RendererGpuVideoDecoderFactories();

  media::VideoDecodeAccelerator* CreateVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Profile profile,
      media::VideoDecodeAccelerator::Client* client) OVERRIDE;

  virtual bool CreateTextures(int32 count, const gfx::Size& size,
                              std::vector<uint32>* texture_ids) OVERRIDE;

  virtual bool DeleteTexture(uint32 texture_id) OVERRIDE;

  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;

 private:
  scoped_refptr<GpuChannelHost> gpu_channel_host_;
  base::WeakPtr<RendererGLContext> context_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(RendererGpuVideoDecoderFactories);
};

#endif  // CONTENT_RENDERER_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
