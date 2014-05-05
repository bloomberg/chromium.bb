// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fake_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/test_helpers.h"

namespace media {

FakeVideoDecoder::FakeVideoDecoder(int decoding_delay,
                                   bool supports_get_decode_output,
                                   int max_parallel_decoding_requests)
    : decoding_delay_(decoding_delay),
      supports_get_decode_output_(supports_get_decode_output),
      max_parallel_decoding_requests_(max_parallel_decoding_requests),
      state_(STATE_UNINITIALIZED),
      hold_decode_(false),
      total_bytes_decoded_(0),
      weak_factory_(this) {
  DCHECK_GE(decoding_delay, 0);
}

FakeVideoDecoder::~FakeVideoDecoder() {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
}

void FakeVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  const PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(config.IsValidConfig());
  DCHECK(held_decode_callbacks_.empty())
      << "No reinitialization during pending decode.";
  DCHECK(reset_cb_.IsNull()) << "No reinitialization during pending reset.";

  current_config_ = config;
  init_cb_.SetCallback(BindToCurrentLoop(status_cb));

  if (!decoded_frames_.empty()) {
    DVLOG(1) << "Decoded frames dropped during reinitialization.";
    decoded_frames_.clear();
  }

  state_ = STATE_NORMAL;
  init_cb_.RunOrHold(PIPELINE_OK);
}

void FakeVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(reset_cb_.IsNull());
  DCHECK_LE(decoded_frames_.size(),
            decoding_delay_ + held_decode_callbacks_.size());
  DCHECK_LT(static_cast<int>(held_decode_callbacks_.size()),
            max_parallel_decoding_requests_);

  int buffer_size = buffer->end_of_stream() ? 0 : buffer->data_size();
  DecodeCB wrapped_decode_cb =
      BindToCurrentLoop(base::Bind(&FakeVideoDecoder::OnFrameDecoded,
                                   weak_factory_.GetWeakPtr(),
                                   buffer_size,
                                   decode_cb));

  if (state_ == STATE_ERROR) {
    wrapped_decode_cb.Run(kDecodeError, scoped_refptr<VideoFrame>());
    return;
  }

  if (buffer->end_of_stream()) {
    state_ = STATE_END_OF_STREAM;
  } else {
    DCHECK(VerifyFakeVideoBufferForTest(buffer, current_config_));
    scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateColorFrame(
        current_config_.coded_size(), 0, 0, 0, buffer->timestamp());
    decoded_frames_.push_back(video_frame);
  }

  RunOrHoldDecode(wrapped_decode_cb);
}

void FakeVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(reset_cb_.IsNull());

  reset_cb_.SetCallback(BindToCurrentLoop(closure));
  decoded_frames_.clear();

  // Defer the reset if a decode is pending.
  if (!held_decode_callbacks_.empty())
    return;

  DoReset();
}

void FakeVideoDecoder::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!init_cb_.IsNull())
    SatisfyInit();
  if (!held_decode_callbacks_.empty())
    SatisfyDecode();
  if (!reset_cb_.IsNull())
    SatisfyReset();

  decoded_frames_.clear();
  state_ = STATE_UNINITIALIZED;
}

scoped_refptr<VideoFrame> FakeVideoDecoder::GetDecodeOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!supports_get_decode_output_ || decoded_frames_.empty())
    return NULL;
  scoped_refptr<VideoFrame> out = decoded_frames_.front();
  decoded_frames_.pop_front();
  return out;
}

void FakeVideoDecoder::HoldNextInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  init_cb_.HoldCallback();
}

void FakeVideoDecoder::HoldDecode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  hold_decode_ = true;
}

void FakeVideoDecoder::HoldNextReset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  reset_cb_.HoldCallback();
}

void FakeVideoDecoder::SatisfyInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(held_decode_callbacks_.empty());
  DCHECK(reset_cb_.IsNull());

  init_cb_.RunHeldCallback();
}

void FakeVideoDecoder::SatisfyDecode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(hold_decode_);

  hold_decode_ = false;

  while (!held_decode_callbacks_.empty()) {
    SatisfySingleDecode();
  }
}

void FakeVideoDecoder::SatisfySingleDecode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!held_decode_callbacks_.empty());

  DecodeCB decode_cb = held_decode_callbacks_.front();
  held_decode_callbacks_.pop_front();
  RunDecodeCallback(decode_cb);

  if (!reset_cb_.IsNull() && held_decode_callbacks_.empty())
    DoReset();
}

void FakeVideoDecoder::SatisfyReset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(held_decode_callbacks_.empty());
  reset_cb_.RunHeldCallback();
}

void FakeVideoDecoder::SimulateError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  state_ = STATE_ERROR;
  while (!held_decode_callbacks_.empty()) {
    held_decode_callbacks_.front().Run(kDecodeError,
                                       scoped_refptr<VideoFrame>());
    held_decode_callbacks_.pop_front();
  }
  decoded_frames_.clear();
}

int FakeVideoDecoder::GetMaxDecodeRequests() const {
  return max_parallel_decoding_requests_;
}

void FakeVideoDecoder::OnFrameDecoded(
    int buffer_size,
    const DecodeCB& decode_cb,
    Status status,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (status == kOk || status == kNotEnoughData)
    total_bytes_decoded_ += buffer_size;
  decode_cb.Run(status, video_frame);
}

void FakeVideoDecoder::RunOrHoldDecode(const DecodeCB& decode_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (hold_decode_) {
    held_decode_callbacks_.push_back(decode_cb);
  } else {
    DCHECK(held_decode_callbacks_.empty());
    RunDecodeCallback(decode_cb);
  }
}

void FakeVideoDecoder::RunDecodeCallback(const DecodeCB& decode_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!reset_cb_.IsNull()) {
    DCHECK(decoded_frames_.empty());
    decode_cb.Run(kAborted, scoped_refptr<VideoFrame>());
    return;
  }

  // Make sure we leave decoding_delay_ frames in the queue and also frames for
  // all pending decode callbacks, except the current one.
  if (decoded_frames_.size() <=
          decoding_delay_ + held_decode_callbacks_.size() &&
      state_ != STATE_END_OF_STREAM) {
    decode_cb.Run(kNotEnoughData, scoped_refptr<VideoFrame>());
    return;
  }

  scoped_refptr<VideoFrame> frame;
  if (decoded_frames_.empty()) {
    DCHECK_EQ(state_, STATE_END_OF_STREAM);
    frame = VideoFrame::CreateEOSFrame();
  } else {
    frame = decoded_frames_.front();
    decoded_frames_.pop_front();
  }
  decode_cb.Run(kOk, frame);
}

void FakeVideoDecoder::DoReset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(held_decode_callbacks_.empty());
  DCHECK(!reset_cb_.IsNull());

  reset_cb_.RunOrHold();
}

}  // namespace media
