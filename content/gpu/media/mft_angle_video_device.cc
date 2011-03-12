// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/media/mft_angle_video_device.h"

#include <d3d9.h>

#include "media/base/video_frame.h"
#include "third_party/angle/src/libGLESv2/main.h"

MftAngleVideoDevice::MftAngleVideoDevice()
    : device_(reinterpret_cast<egl::Display*>(
          eglGetCurrentDisplay())->getDevice()) {
}

void* MftAngleVideoDevice::GetDevice() {
  return device_;
}

bool MftAngleVideoDevice::CreateVideoFrameFromGlTextures(
    size_t width, size_t height, media::VideoFrame::Format format,
    const std::vector<media::VideoFrame::GlTexture>& textures,
    scoped_refptr<media::VideoFrame>* frame) {
  media::VideoFrame::GlTexture texture_array[media::VideoFrame::kMaxPlanes];
  memset(texture_array, 0, sizeof(texture_array));

  for (size_t i = 0; i < textures.size(); ++i) {
    texture_array[i] = textures[i];
  }

  media::VideoFrame::CreateFrameGlTexture(format,
                                          width,
                                          height,
                                          texture_array,
                                          frame);
  return *frame != NULL;
}

void MftAngleVideoDevice::ReleaseVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  // We didn't need to anything here because we didn't allocate any resources
  // for the VideoFrame(s) generated.
}

bool MftAngleVideoDevice::ConvertToVideoFrame(
    void* buffer, scoped_refptr<media::VideoFrame> frame) {
  gl::Context* context = (gl::Context*)eglGetCurrentContext();
  // TODO(hclam): Connect ANGLE to upload the surface to texture when changes
  // to ANGLE is done.
  return true;
}
