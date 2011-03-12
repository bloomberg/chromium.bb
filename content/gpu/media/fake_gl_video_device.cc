// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/media/fake_gl_video_device.h"

#include "app/gfx/gl/gl_bindings.h"
#include "media/base/video_frame.h"

void* FakeGlVideoDevice::GetDevice() {
  // No actual hardware device should be used.
  return NULL;
}

bool FakeGlVideoDevice::CreateVideoFrameFromGlTextures(
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

void FakeGlVideoDevice::ReleaseVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  // We didn't need to anything here because we didin't allocate any resources
  // for the VideoFrame(s) generated.
}

bool FakeGlVideoDevice::ConvertToVideoFrame(
    void* buffer, scoped_refptr<media::VideoFrame> frame) {
  // Assume we are in the right context and then upload the content to the
  // texture.
  glBindTexture(GL_TEXTURE_2D,
                frame->gl_texture(media::VideoFrame::kRGBPlane));

  // |buffer| is also a VideoFrame.
  scoped_refptr<media::VideoFrame> frame_to_upload(
      reinterpret_cast<media::VideoFrame*>(buffer));
  DCHECK_EQ(frame->width(), frame_to_upload->width());
  DCHECK_EQ(frame->height(), frame_to_upload->height());
  DCHECK_EQ(frame->format(), frame_to_upload->format());
  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, frame_to_upload->width(),
      frame_to_upload->height(), 0, GL_RGBA,
      GL_UNSIGNED_BYTE, frame_to_upload->data(media::VideoFrame::kRGBPlane));
  return true;
}
