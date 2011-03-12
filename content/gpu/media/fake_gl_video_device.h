// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_
#define CONTENT_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_

#include "content/gpu/media/gpu_video_device.h"

// A simple GpuVideoDevice that create VideoFrame with GL textures.
// It uploads frames in RGBA format in system memory to the GL texture.
class FakeGlVideoDevice : public GpuVideoDevice {
 public:
  virtual ~FakeGlVideoDevice() {}

  virtual void* GetDevice();
  virtual bool CreateVideoFrameFromGlTextures(
      size_t width, size_t height, media::VideoFrame::Format format,
      const std::vector<media::VideoFrame::GlTexture>& textures,
      scoped_refptr<media::VideoFrame>* frame);
  virtual void ReleaseVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame);
  virtual bool ConvertToVideoFrame(void* buffer,
                                   scoped_refptr<media::VideoFrame> frame);
};

#endif  // CONTENT_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_
