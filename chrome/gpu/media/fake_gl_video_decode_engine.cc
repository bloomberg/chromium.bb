// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/media/fake_gl_video_decode_engine.h"

#include "app/gfx/gl/gl_bindings.h"

FakeGlVideoDecodeEngine::FakeGlVideoDecodeEngine()
    : width_(0),
      height_(0),
      handler_(NULL) {
}

FakeGlVideoDecodeEngine::~FakeGlVideoDecodeEngine() {
}

void FakeGlVideoDecodeEngine::Initialize(
    MessageLoop* message_loop,
    media::VideoDecodeEngine::EventHandler* event_handler,
    const media::VideoCodecConfig& config) {
  handler_ = event_handler;
  width_ = config.width;
  height_ = config.height;

  media::VideoCodecInfo info;
  info.success = true;
  info.provides_buffers = true;
  info.stream_info.surface_format = media::VideoFrame::RGBA;
  info.stream_info.surface_type = media::VideoFrame::TYPE_GL_TEXTURE;
  info.stream_info.surface_width = width_;
  info.stream_info.surface_height = height_;

  // TODO(hclam): When we have VideoDecodeContext we should use it to allocate
  // video frames.
  handler_->OnInitializeComplete(info);
}

void FakeGlVideoDecodeEngine::Uninitialize() {
  handler_->OnUninitializeComplete();
}

void FakeGlVideoDecodeEngine::Flush() {
  handler_->OnFlushComplete();
}

void FakeGlVideoDecodeEngine::Seek() {
  handler_->OnSeekComplete();
}

void FakeGlVideoDecodeEngine::ConsumeVideoSample(
    scoped_refptr<media::Buffer> buffer) {
  // Don't care.
}

void FakeGlVideoDecodeEngine::ProduceVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  // Fake that we need some buffer.
  handler_->ProduceVideoSample(NULL);

  int size = width_ * height_ * 4;
  scoped_array<uint8> buffer(new uint8[size]);
  memset(buffer.get(), 0, size);

  uint8* row = buffer.get();
  static int seed = 0;

  for (int y = 0; y < height_; ++y) {
    int offset = y % 3;
    for (int x = 0; x < width_; ++x) {
      row[x * 4 + offset] = seed++;
      seed &= 255;
    }
    row += width_ * 4;
  }
  ++seed;

  // Assume we are in the right context and then upload the content to the
  // texture.
  glBindTexture(GL_TEXTURE_2D,
                frame->gl_texture(media::VideoFrame::kRGBPlane));
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, buffer.get());

  // We have done generating data to the frame so give it to the handler.
  // TODO(hclam): Advance the timestamp every time we call this method.
  handler_->ConsumeVideoFrame(frame);
}
