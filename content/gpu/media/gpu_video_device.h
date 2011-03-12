// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_MEDIA_GPU_VIDEO_DEVICE_H_
#define CONTENT_GPU_MEDIA_GPU_VIDEO_DEVICE_H_

#include <vector>

#include "media/base/video_frame.h"
#include "media/video/video_decode_context.h"

// A GpuVideoDevice is used by GpuVideoDecoder to allocate video frames
// meaningful to a corresponding VideoDecodeEngine.
//
// GpuVideoDecoder will provide a set of GL textures to this class and then
// GpuVideoDevice will transform the textures into a set of VideoFrame
// objects that can be used by VideoDecodeEngine.
//
// See text in GpuVideoDecoder for the overall flow for buffer allocation.
//
// Since all graphics commands execute on the main thread in the GPU process
// all the methods provided by this class are synchronous.
class GpuVideoDevice {
 public:
  virtual ~GpuVideoDevice() {}

  // Get the hardware video decoding device handle.
  virtual void* GetDevice() = 0;

  // The following method is used by GpuVideoDecoder to create VideoFrame(s)
  // associated with some GL textures.
  //
  // VideoFrame generated is used by VideoDecodeEngine for output buffer.
  //
  // |frame| will contain the VideoFrame generated.
  //
  // Return true if the operation was successful.
  virtual bool CreateVideoFrameFromGlTextures(
      size_t width, size_t height, media::VideoFrame::Format format,
      const std::vector<media::VideoFrame::GlTexture>& textures,
      scoped_refptr<media::VideoFrame>* frame) = 0;

  // Release VideoFrame generated.
  virtual void ReleaseVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) = 0;

  // Upload a device specific buffer to a VideoFrame object that can be used in
  // the GPU process.
  //
  // Return true if successful.
  virtual bool ConvertToVideoFrame(void* buffer,
                                   scoped_refptr<media::VideoFrame> frame) = 0;
};

#endif  // CONTENT_GPU_MEDIA_GPU_VIDEO_DEVICE_H_
