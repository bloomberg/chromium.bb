// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_
#define CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_

#include "chrome/gpu/media/gpu_video_device.h"

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
  virtual bool UploadToVideoFrame(void* buffer,
                                  scoped_refptr<media::VideoFrame> frame);
};

#endif  // CHROME_GPU_MEDIA_FAKE_GL_VIDEO_DEVICE_H_
