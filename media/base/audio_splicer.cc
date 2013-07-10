// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_splicer.h"

#include <cstdlib>

#include "base/logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"

namespace media {

// Largest gap or overlap allowed by this class. Anything
// larger than this will trigger an error.
// This is an arbitrary value, but the initial selection of 50ms
// roughly represents the duration of 2 compressed AAC or MP3 frames.
static const int kMaxTimeDeltaInMilliseconds = 50;

AudioSplicer::AudioSplicer(int samples_per_second)
    : output_timestamp_helper_(samples_per_second),
      min_gap_size_(2),
      received_end_of_stream_(false) {
}

AudioSplicer::~AudioSplicer() {
}

void AudioSplicer::Reset() {
  output_timestamp_helper_.SetBaseTimestamp(kNoTimestamp());
  output_buffers_.clear();
  received_end_of_stream_ = false;
}

bool AudioSplicer::AddInput(const scoped_refptr<AudioBuffer>& input) {
  DCHECK(!received_end_of_stream_ || input->end_of_stream());

  if (input->end_of_stream()) {
    output_buffers_.push_back(input);
    received_end_of_stream_ = true;
    return true;
  }

  DCHECK(input->timestamp() != kNoTimestamp());
  DCHECK(input->duration() > base::TimeDelta());
  DCHECK_GT(input->frame_count(), 0);

  if (output_timestamp_helper_.base_timestamp() == kNoTimestamp())
    output_timestamp_helper_.SetBaseTimestamp(input->timestamp());

  if (output_timestamp_helper_.base_timestamp() > input->timestamp()) {
    DVLOG(1) << "Input timestamp is before the base timestamp.";
    return false;
  }

  base::TimeDelta timestamp = input->timestamp();
  base::TimeDelta expected_timestamp = output_timestamp_helper_.GetTimestamp();
  base::TimeDelta delta = timestamp - expected_timestamp;

  if (std::abs(delta.InMilliseconds()) > kMaxTimeDeltaInMilliseconds) {
    DVLOG(1) << "Timestamp delta too large: " << delta.InMicroseconds() << "us";
    return false;
  }

  int frames_to_fill = 0;
  if (delta != base::TimeDelta())
    frames_to_fill = output_timestamp_helper_.GetFramesToTarget(timestamp);

  if (frames_to_fill == 0 || std::abs(frames_to_fill) < min_gap_size_) {
    AddOutputBuffer(input);
    return true;
  }

  if (frames_to_fill > 0) {
    DVLOG(1) << "Gap detected @ " << expected_timestamp.InMicroseconds()
             << " us: " << delta.InMicroseconds() << " us";

    // Create a buffer with enough silence samples to fill the gap and
    // add it to the output buffer.
    scoped_refptr<AudioBuffer> gap = AudioBuffer::CreateEmptyBuffer(
        input->channel_count(),
        frames_to_fill,
        expected_timestamp,
        output_timestamp_helper_.GetFrameDuration(frames_to_fill));
    AddOutputBuffer(gap);

    // Add the input buffer now that the gap has been filled.
    AddOutputBuffer(input);
    return true;
  }

  int frames_to_skip = -frames_to_fill;

  DVLOG(1) << "Overlap detected @ " << expected_timestamp.InMicroseconds()
           << " us: "  << -delta.InMicroseconds() << " us";

  if (input->frame_count() <= frames_to_skip) {
    DVLOG(1) << "Dropping whole buffer";
    return true;
  }

  // Copy the trailing samples that do not overlap samples already output
  // into a new buffer. Add this new buffer to the output queue.
  //
  // TODO(acolwell): Implement a cross-fade here so the transition is less
  // jarring.
  input->TrimStart(frames_to_skip);
  AddOutputBuffer(input);
  return true;
}

bool AudioSplicer::HasNextBuffer() const {
  return !output_buffers_.empty();
}

scoped_refptr<AudioBuffer> AudioSplicer::GetNextBuffer() {
  scoped_refptr<AudioBuffer> ret = output_buffers_.front();
  output_buffers_.pop_front();
  return ret;
}

void AudioSplicer::AddOutputBuffer(const scoped_refptr<AudioBuffer>& buffer) {
  output_timestamp_helper_.AddFrames(buffer->frame_count());
  output_buffers_.push_back(buffer);
}

}  // namespace media
