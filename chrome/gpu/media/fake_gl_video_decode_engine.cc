// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/media/fake_gl_video_decode_engine.h"

#include "media/base/video_frame.h"
#include "media/video/video_decode_context.h"

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
    media::VideoDecodeContext* context,
    const media::VideoCodecConfig& config) {
  handler_ = event_handler;
  context_ = context;
  width_ = config.width;
  height_ = config.height;

  // Create an internal VideoFrame that we can write to. This is going to be
  // uploaded through VideoDecodeContext.
  media::VideoFrame::CreateFrame(
      media::VideoFrame::RGBA, width_, height_, base::TimeDelta(),
      base::TimeDelta(), &internal_frame_);
  memset(internal_frame_->data(media::VideoFrame::kRGBPlane), 0,
         height_ * internal_frame_->stride(media::VideoFrame::kRGBPlane));

  // Use VideoDecodeContext to allocate VideoFrame that can be consumed by
  // external body.
  context_->AllocateVideoFrames(
      1, width_, height_, media::VideoFrame::RGBA, &external_frames_,
      NewRunnableMethod(this,
                        &FakeGlVideoDecodeEngine::AllocationCompleteTask));
}

void FakeGlVideoDecodeEngine::AllocationCompleteTask() {
  DCHECK_EQ(1u, external_frames_.size());
  DCHECK_EQ(media::VideoFrame::TYPE_GL_TEXTURE, external_frames_[0]->type());

  media::VideoCodecInfo info;
  info.success = true;
  info.provides_buffers = true;
  info.stream_info.surface_format = media::VideoFrame::RGBA;
  info.stream_info.surface_type = media::VideoFrame::TYPE_GL_TEXTURE;
  info.stream_info.surface_width = width_;
  info.stream_info.surface_height = height_;
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

  uint8* row = internal_frame_->data(media::VideoFrame::kRGBPlane);
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

  // After we have filled the content upload the internal frame to the
  // VideoFrame allocated through VideoDecodeContext.
  context_->UploadToVideoFrame(
      internal_frame_, external_frames_[0],
      NewRunnableMethod(this, &FakeGlVideoDecodeEngine::UploadCompleteTask,
                        external_frames_[0]));
}

void FakeGlVideoDecodeEngine::UploadCompleteTask(
    scoped_refptr<media::VideoFrame> frame) {
  // |frame| is the upload target. We can immediately send this frame out.
  handler_->ConsumeVideoFrame(frame);
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(FakeGlVideoDecodeEngine);
