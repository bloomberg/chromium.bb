// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_MEDIA_MFT_ANGLE_VIDEO_DEVICE_H_
#define CONTENT_GPU_MEDIA_MFT_ANGLE_VIDEO_DEVICE_H_

#include "base/scoped_comptr_win.h"
#include "content/gpu/media/gpu_video_device.h"

struct IDirect3DDevice9;
extern "C" const GUID IID_IDirect3DDevice9;

namespace media {
class VideoFrame;
}  // namespace media

// This class is used to provide hardware video device, video frames and
// allow video frames to be uploaded to their final render target.
//
// This specifically serves MftH264DecodeEngine in the context of ANGLE.
class MftAngleVideoDevice : public GpuVideoDevice {
 public:
  MftAngleVideoDevice();
  virtual ~MftAngleVideoDevice() {}

  // GpuVideoDevice implementation.
  virtual void* GetDevice();
  virtual bool CreateVideoFrameFromGlTextures(
      size_t width, size_t height, media::VideoFrame::Format format,
      const std::vector<media::VideoFrame::GlTexture>& textures,
      scoped_refptr<media::VideoFrame>* frame);
  virtual void ReleaseVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame);
  virtual bool ConvertToVideoFrame(void* buffer,
                                   scoped_refptr<media::VideoFrame> frame);

 private:
  ScopedComPtr<IDirect3DDevice9, &IID_IDirect3DDevice9> device_;
};

#endif  // CONTENT_GPU_MEDIA_MFT_ANGLE_VIDEO_DEVICE_H_
