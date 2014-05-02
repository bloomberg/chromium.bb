// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_discard_helper.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"

namespace media {

static void WarnOnNonMonotonicTimestamps(base::TimeDelta last_timestamp,
                                         base::TimeDelta current_timestamp) {
  if (last_timestamp == kNoTimestamp() || last_timestamp < current_timestamp)
    return;

  const base::TimeDelta diff = current_timestamp - last_timestamp;
  DLOG(WARNING) << "Input timestamps are not monotonically increasing! "
                << " ts " << current_timestamp.InMicroseconds() << " us"
                << " diff " << diff.InMicroseconds() << " us";
}

AudioDiscardHelper::AudioDiscardHelper(int sample_rate)
    : sample_rate_(sample_rate),
      timestamp_helper_(sample_rate_),
      discard_frames_(0),
      last_input_timestamp_(kNoTimestamp()) {
  DCHECK_GT(sample_rate_, 0);
}

AudioDiscardHelper::~AudioDiscardHelper() {
}

size_t AudioDiscardHelper::TimeDeltaToFrames(base::TimeDelta duration) const {
  DCHECK(duration >= base::TimeDelta());
  return duration.InSecondsF() * sample_rate_ + 0.5;
}

void AudioDiscardHelper::Reset(size_t initial_discard) {
  discard_frames_ = initial_discard;
  last_input_timestamp_ = kNoTimestamp();
  timestamp_helper_.SetBaseTimestamp(kNoTimestamp());
}

bool AudioDiscardHelper::ProcessBuffers(
    const scoped_refptr<DecoderBuffer>& encoded_buffer,
    const scoped_refptr<AudioBuffer>& decoded_buffer) {
  DCHECK(!encoded_buffer->end_of_stream());
  DCHECK(encoded_buffer->timestamp() != kNoTimestamp());

  // Issue a debug warning when we see non-monotonic timestamps.  Only a warning
  // to allow chained OGG playback.
  WarnOnNonMonotonicTimestamps(last_input_timestamp_,
                               encoded_buffer->timestamp());
  last_input_timestamp_ = encoded_buffer->timestamp();

  // If this is the first buffer seen, setup the timestamp helper.
  if (!initialized()) {
    // Clamp the base timestamp to zero.
    timestamp_helper_.SetBaseTimestamp(
        std::max(base::TimeDelta(), encoded_buffer->timestamp()));
  }
  DCHECK(initialized());

  if (!decoded_buffer || !decoded_buffer->frame_count())
    return false;

  if (discard_frames_ > 0) {
    const size_t decoded_frames = decoded_buffer->frame_count();
    const size_t frames_to_discard = std::min(discard_frames_, decoded_frames);
    discard_frames_ -= frames_to_discard;

    // If everything would be discarded, indicate a new buffer is required.
    if (frames_to_discard == decoded_frames)
      return false;

    decoded_buffer->TrimStart(frames_to_discard);
  }

  // TODO(dalecurtis): Applying the current buffer's discard padding doesn't
  // make sense in the Vorbis case because there is a delay of one buffer before
  // decoded buffers are returned. Fix and add support for more than just end
  // trimming.  See http://crbug.com/360961.
  if (encoded_buffer->discard_padding() > base::TimeDelta()) {
    const size_t decoded_frames = decoded_buffer->frame_count();
    const size_t end_frames_to_discard =
        TimeDeltaToFrames(encoded_buffer->discard_padding());
    if (end_frames_to_discard > decoded_frames) {
      DLOG(ERROR) << "Encountered invalid discard padding value.";
      return false;
    }

    // If everything would be discarded, indicate a new buffer is required.
    if (end_frames_to_discard == decoded_frames)
      return false;

    decoded_buffer->TrimEnd(end_frames_to_discard);
  } else {
    DCHECK(encoded_buffer->discard_padding() == base::TimeDelta());
  }

  // Assign timestamp and duration to the buffer.
  decoded_buffer->set_timestamp(timestamp_helper_.GetTimestamp());
  decoded_buffer->set_duration(
      timestamp_helper_.GetFrameDuration(decoded_buffer->frame_count()));
  timestamp_helper_.AddFrames(decoded_buffer->frame_count());
  return true;
}

}  // namespace media
