// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_video_encode_accelerator.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace media {
namespace cast {
namespace test {

static const unsigned int kMinimumInputCount = 1;
static const size_t kMinimumOutputBufferSize = 123456;

FakeVideoEncodeAccelerator::FakeVideoEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      client_(NULL),
      first_(true),
      weak_this_factory_(this) {}

FakeVideoEncodeAccelerator::~FakeVideoEncodeAccelerator() {
  weak_this_factory_.InvalidateWeakPtrs();
}

bool FakeVideoEncodeAccelerator::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  client_ = client;
  if (output_profile != media::VP8PROFILE_MAIN &&
      output_profile != media::H264PROFILE_MAIN) {
    return false;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoEncodeAccelerator::DoRequireBitstreamBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 kMinimumInputCount,
                 input_visible_size,
                 kMinimumOutputBufferSize));
  return true;
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
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoEncodeAccelerator::DoBitstreamBufferReady,
                 weak_this_factory_.GetWeakPtr(),
                 id,
                 kMinimumOutputBufferSize,
                 is_key_fame));
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

void FakeVideoEncodeAccelerator::SendDummyFrameForTesting(bool key_frame) {
  DoBitstreamBufferReady(0, 23, key_frame);
}

void FakeVideoEncodeAccelerator::DoRequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) const {
  client_->RequireBitstreamBuffers(
      input_count, input_coded_size, output_buffer_size);
}

void FakeVideoEncodeAccelerator::DoBitstreamBufferReady(
    int32 bitstream_buffer_id,
    size_t payload_size,
    bool key_frame) const {
  client_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame);
}

}  // namespace test
}  // namespace cast
}  // namespace media
