// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fake_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/test_helpers.h"

namespace media {

FakeVideoDecoder::FakeVideoDecoder(int decoding_delay)
    : message_loop_(base::MessageLoopProxy::current()),
      weak_factory_(this),
      decoding_delay_(decoding_delay),
      state_(UNINITIALIZED),
      total_bytes_decoded_(0) {
  DCHECK_GE(decoding_delay, 0);
}

FakeVideoDecoder::~FakeVideoDecoder() {
  DCHECK_EQ(state_, UNINITIALIZED);
}

void FakeVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  const PipelineStatusCB& status_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());
  DCHECK(read_cb_.IsNull()) << "No reinitialization during pending read.";
  DCHECK(reset_cb_.IsNull()) << "No reinitialization during pending reset.";

  weak_this_ = weak_factory_.GetWeakPtr();

  current_config_ = config;
  init_cb_.SetCallback(BindToCurrentLoop(status_cb));

  if (!decoded_frames_.empty()) {
    DVLOG(1) << "Decoded frames dropped during reinitialization.";
    decoded_frames_.clear();
  }

  state_ = NORMAL;
  init_cb_.RunOrHold(PIPELINE_OK);
}

void FakeVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull()) << "Overlapping decodes are not supported.";
  DCHECK(reset_cb_.IsNull());
  DCHECK_LE(decoded_frames_.size(), static_cast<size_t>(decoding_delay_));

  int buffer_size = buffer->IsEndOfStream() ? 0 : buffer->GetDataSize();
  read_cb_.SetCallback(BindToCurrentLoop(base::Bind(
      &FakeVideoDecoder::OnFrameDecoded, weak_this_, buffer_size, read_cb)));

  if (buffer->IsEndOfStream() && decoded_frames_.empty()) {
    read_cb_.RunOrHold(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  if (!buffer->IsEndOfStream()) {
    DCHECK(VerifyFakeVideoBufferForTest(buffer, current_config_));
    scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateColorFrame(
        current_config_.coded_size(), 0, 0, 0, buffer->GetTimestamp());
    decoded_frames_.push_back(video_frame);

    if (decoded_frames_.size() <= static_cast<size_t>(decoding_delay_)) {
      read_cb_.RunOrHold(kNotEnoughData, scoped_refptr<VideoFrame>());
      return;
    }
  }

  scoped_refptr<VideoFrame> frame = decoded_frames_.front();
  decoded_frames_.pop_front();
  read_cb_.RunOrHold(kOk, frame);
}

void FakeVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(reset_cb_.IsNull());
  reset_cb_.SetCallback(BindToCurrentLoop(closure));

  // Defer the reset if a read is pending.
  if (!read_cb_.IsNull())
    return;

  DoReset();
}

void FakeVideoDecoder::Stop(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  stop_cb_.SetCallback(BindToCurrentLoop(closure));

  // Defer the stop if an init, a read or a reset is pending.
  if (!init_cb_.IsNull() || !read_cb_.IsNull() || !reset_cb_.IsNull())
    return;

  DoStop();
}

void FakeVideoDecoder::HoldNextInit() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  init_cb_.HoldCallback();
}

void FakeVideoDecoder::HoldNextRead() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  read_cb_.HoldCallback();
}

void FakeVideoDecoder::HoldNextReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  reset_cb_.HoldCallback();
}

void FakeVideoDecoder::HoldNextStop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  stop_cb_.HoldCallback();
}

void FakeVideoDecoder::SatisfyInit() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull());
  DCHECK(reset_cb_.IsNull());

  init_cb_.RunHeldCallback();

  if (!stop_cb_.IsNull())
    DoStop();
}

void FakeVideoDecoder::SatisfyRead() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  read_cb_.RunHeldCallback();

  if (!reset_cb_.IsNull())
    DoReset();

  if (reset_cb_.IsNull() && !stop_cb_.IsNull())
    DoStop();
}

void FakeVideoDecoder::SatisfyReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull());
  reset_cb_.RunHeldCallback();

  if (!stop_cb_.IsNull())
    DoStop();
}

void FakeVideoDecoder::SatisfyStop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull());
  DCHECK(reset_cb_.IsNull());
  stop_cb_.RunHeldCallback();
}

void FakeVideoDecoder::DoReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull());
  DCHECK(!reset_cb_.IsNull());

  decoded_frames_.clear();
  reset_cb_.RunOrHold();
}

void FakeVideoDecoder::DoStop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_cb_.IsNull());
  DCHECK(reset_cb_.IsNull());
  DCHECK(!stop_cb_.IsNull());

  state_ = UNINITIALIZED;
  decoded_frames_.clear();
  stop_cb_.RunOrHold();
}

void FakeVideoDecoder::OnFrameDecoded(
    int buffer_size,
    const ReadCB& read_cb,
    Status status,
    const scoped_refptr<VideoFrame>& video_frame) {
  if (status == kOk || status == kNotEnoughData)
    total_bytes_decoded_ += buffer_size;
  read_cb.Run(status, video_frame);
}

}  // namespace media
