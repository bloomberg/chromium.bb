// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/media/fake_gl_video_decode_engine.h"

#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_context.h"

FakeGlVideoDecodeEngine::FakeGlVideoDecodeEngine()
    : width_(0),
      height_(0),
      handler_(NULL),
      context_(NULL) {
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
  // TODO(hclam): It is horrible to use constants everywhere in the code!
  // The number of frames come from VideoRendererBase in the renderer, this is
  // horrible!
  context_->AllocateVideoFrames(
      media::Limits::kMaxVideoFrames, width_, height_, media::VideoFrame::RGBA,
      &external_frames_,
      NewRunnableMethod(this,
                        &FakeGlVideoDecodeEngine::AllocationCompleteTask));
}

void FakeGlVideoDecodeEngine::AllocationCompleteTask() {
  DCHECK(media::Limits::kMaxVideoFrames == external_frames_.size());
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
  // TODO(hclam): This is a big hack to continue playing because we need to
  // *push* decoded frames to downstream. The model used in VideoRendererBase
  // to wait for *push* is flawed.
  for (size_t i = 0; i < external_frames_.size(); ++i)
    handler_->ConsumeVideoFrame(external_frames_[i], dummy_stats_);
  handler_->OnSeekComplete();
}

void FakeGlVideoDecodeEngine::ConsumeVideoSample(
    scoped_refptr<media::Buffer> sample) {
  DCHECK(!pending_frames_.empty());
  scoped_refptr<media::VideoFrame> frame = pending_frames_.front();
  pending_frames_.pop();

  frame->SetDuration(sample->GetDuration());
  frame->SetTimestamp(sample->GetTimestamp());

  // Generate a pattern to the internal frame and then uploads it.
  int size = width_ * height_ * 4;
  scoped_array<uint8> buffer(new uint8[size]);
  memset(buffer.get(), 255, size);

  uint8* row = internal_frame_->data(media::VideoFrame::kRGBPlane);
  static int seed = 0;

  for (int y = 0; y < height_; ++y) {
    int offset = y % 3;
    for (int x = 0; x < width_; ++x) {
      row[x * 4 + offset + 1] = seed++;
      seed &= 255;
    }
    row += width_ * 4;
  }
  ++seed;

  // After we have filled the content upload the internal frame to the
  // VideoFrame allocated through VideoDecodeContext.
  context_->ConvertToVideoFrame(
      internal_frame_, frame,
      NewRunnableMethod(this, &FakeGlVideoDecodeEngine::UploadCompleteTask,
                        frame));
}

void FakeGlVideoDecodeEngine::ProduceVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  // Enqueue the frame to the pending queue.
  pending_frames_.push(frame);

  // Fake that we need some buffer.
  handler_->ProduceVideoSample(NULL);
}

void FakeGlVideoDecodeEngine::UploadCompleteTask(
    scoped_refptr<media::VideoFrame> frame) {
  // |frame| is the upload target. We can immediately send this frame out.
  handler_->ConsumeVideoFrame(frame, dummy_stats_);
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(FakeGlVideoDecodeEngine);
