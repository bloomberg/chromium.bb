// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_video_encode_accelerator.h"

#include "base/logging.h"

namespace media {
namespace cast {
namespace test {

static const unsigned int kMinimumInputCount = 1;
static const size_t kMinimumOutputBufferSize = 123456;

FakeVideoEncodeAccelerator::FakeVideoEncodeAccelerator()
    : client_(NULL), first_(true) {}

FakeVideoEncodeAccelerator::~FakeVideoEncodeAccelerator() {}

void FakeVideoEncodeAccelerator::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  client_ = client;
  if (output_profile != media::VP8PROFILE_MAIN &&
      output_profile != media::H264PROFILE_MAIN) {
    client_->NotifyError(kInvalidArgumentError);
    return;
  }
  client_->NotifyInitializeDone();
  client_->RequireBitstreamBuffers(
      kMinimumInputCount, input_visible_size, kMinimumOutputBufferSize);
}

void FakeVideoEncodeAccelerator::Encode(const scoped_refptr<VideoFrame>& frame,
                                        bool force_keyframe) {
  DCHECK(client_);
  DCHECK(!available_buffer_ids_.empty());

  // Fake that we have encoded the frame; resulting in using the full output
  // buffer.
  int32 id = available_buffer_ids_.front();
  available_buffer_ids_.pop_front();

  bool is_key_fame = force_keyframe;
  if (first_) {
    is_key_fame = true;
    first_ = false;
  }
  client_->BitstreamBufferReady(id, kMinimumOutputBufferSize, is_key_fame);
}

void FakeVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  available_buffer_ids_.push_back(buffer.id());
}

void FakeVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  // No-op.
}

void FakeVideoEncodeAccelerator::Destroy() { delete this; }

}  // namespace test
}  // namespace cast
}  // namespace media
