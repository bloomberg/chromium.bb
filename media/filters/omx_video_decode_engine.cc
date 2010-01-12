// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/omx_video_decode_engine.h"

#include "media/base/callback.h"
#include "media/filters/ffmpeg_common.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_codec.h"

namespace media {

OmxVideoDecodeEngine::OmxVideoDecodeEngine()
    : state_(kCreated),
      frame_bytes_(0),
      has_fed_on_eos_(false),
      message_loop_(NULL) {
}

OmxVideoDecodeEngine::~OmxVideoDecodeEngine() {
}

void OmxVideoDecodeEngine::Initialize(AVStream* stream, Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);
  omx_codec_ = new media::OmxCodec(message_loop_);

  width_ = stream->codec->width;
  height_ = stream->codec->height;

  // TODO(ajwong): Extract magic formula to something based on output
  // pixel format.
  frame_bytes_ = (width_ * height_ * 3) / 2;

  // TODO(ajwong): Find the right way to determine the Omx component name.
  OmxCodec::OmxMediaFormat input_format;
  input_format.codec = OmxCodec::kCodecH264;
  OmxCodec::OmxMediaFormat output_format;
  output_format.codec = OmxCodec::kCodecRaw;
  omx_codec_->Setup("OMX.st.video_decoder.avc", input_format, output_format);
  omx_codec_->SetErrorCallback(
      NewCallback(this, &OmxVideoDecodeEngine::OnHardwareError));
  omx_codec_->Start();
  state_ = kNormal;
}

void OmxVideoDecodeEngine::OnHardwareError() {
  // TODO(ajwong): Threading?
  state_ = kError;
}

void OmxVideoDecodeEngine::DecodeFrame(const Buffer& buffer,
                                       AVFrame* yuv_frame,
                                       bool* got_result,
                                       Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  if (state_ != kNormal) {
    return;
  }

  if (!has_fed_on_eos_) {
    // TODO(ajwong): This is a memcpy() of the compressed frame. Avoid?
    uint8* data = new uint8[buffer.GetDataSize()];
    memcpy(data, buffer.GetData(), buffer.GetDataSize());

    InputBuffer* input_buffer = new InputBuffer(data, buffer.GetDataSize());

    // Feed in the new buffer regardless.
    //
    // TODO(ajwong): This callback stuff is messy. Cleanup.
    CleanupCallback<OmxCodec::FeedCallback>* feed_done =
        new CleanupCallback<OmxCodec::FeedCallback>(
            NewCallback(this, &OmxVideoDecodeEngine::OnFeedDone));
    feed_done->DeleteWhenDone(input_buffer);
    omx_codec_->Feed(input_buffer, feed_done);

    if (buffer.IsEndOfStream()) {
      has_fed_on_eos_ = true;
    }
  }

  omx_codec_->Read(NewCallback(this, &OmxVideoDecodeEngine::OnReadComplete));

  if (DecodedFrameAvailable()) {
    scoped_ptr<YuvFrame> frame(GetFrame());

    // TODO(ajwong): This is a memcpy(). Avoid this.
    // TODO(ajwong): This leaks memory. Fix by not using AVFrame.
    const size_t frame_pixels = width_ * height_;
    yuv_frame->data[0] = new uint8_t[frame_pixels];
    yuv_frame->data[1] = new uint8_t[frame_pixels / 4];
    yuv_frame->data[2] = new uint8_t[frame_pixels / 4];
    yuv_frame->linesize[0] = width_;
    yuv_frame->linesize[1] = width_ / 2;
    yuv_frame->linesize[2] = width_ / 2;

    memcpy(yuv_frame->data[0], frame->data, frame_pixels);
    memcpy(yuv_frame->data[1], frame->data + frame_pixels, frame_pixels / 4);
    memcpy(yuv_frame->data[2],
           frame->data + frame_pixels + frame_pixels/4,
           frame_pixels / 4);
  }
}

void OmxVideoDecodeEngine::OnFeedDone(InputBuffer* buffer) {
  // TODO(ajwong): Add a DoNothingCallback or similar.
}

void OmxVideoDecodeEngine::Flush(Task* done_cb) {
  AutoLock lock(lock_);
  omx_codec_->Flush(TaskToCallbackAdapter::NewCallback(done_cb));
}

VideoSurface::Format OmxVideoDecodeEngine::GetSurfaceFormat() const {
  return VideoSurface::YV12;
}

void OmxVideoDecodeEngine::Stop(Callback0::Type* done_cb) {
  AutoLock lock(lock_);
  omx_codec_->Stop(done_cb);
  state_ = kStopped;
}

void OmxVideoDecodeEngine::OnReadComplete(uint8* buffer, int size) {
  if ((size_t)size != frame_bytes_) {
    LOG(ERROR) << "Read completed with weird size: " << size;
  }
  MergeBytesFrameQueue(buffer, size);
}

bool OmxVideoDecodeEngine::IsFrameComplete(const YuvFrame* frame) {
  return frame->size == frame_bytes_;
}

bool OmxVideoDecodeEngine::DecodedFrameAvailable() {
  AutoLock lock(lock_);
  return (!yuv_frame_queue_.empty() &&
          IsFrameComplete(yuv_frame_queue_.front()));
}

void OmxVideoDecodeEngine::MergeBytesFrameQueue(uint8* buffer, int size) {
  AutoLock lock(lock_);
  int amount_left = size;

  // TODO(ajwong): Do the swizzle here instead of in DecodeFrame.  This
  // should be able to avoid 1 memcpy.
  while (amount_left > 0) {
    if (yuv_frame_queue_.empty() || IsFrameComplete(yuv_frame_queue_.back())) {
      yuv_frame_queue_.push_back(new YuvFrame(frame_bytes_));
    }
    YuvFrame* frame = yuv_frame_queue_.back();
    int amount_to_copy = std::min((int)(frame_bytes_ - frame->size), size);
    frame->size += amount_to_copy;
    memcpy(frame->data, buffer, amount_to_copy);
    amount_left -= amount_to_copy;
  }
}

OmxVideoDecodeEngine::YuvFrame* OmxVideoDecodeEngine::GetFrame() {
  AutoLock lock(lock_);
  if (yuv_frame_queue_.empty()) {
    return NULL;
  }
  YuvFrame* frame = yuv_frame_queue_.front();
  yuv_frame_queue_.pop_front();
  return frame;
}

}  // namespace media
