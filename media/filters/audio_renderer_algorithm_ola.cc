// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_ola.h"

#include <algorithm>
#include <cmath>

#include "media/base/buffers.h"

namespace media {

// Default window and crossfade lengths in seconds.
const double kDefaultWindowLength = 0.08;
const double kDefaultCrossfadeLength = 0.008;

// Default mute ranges for fast/slow audio. These rates would sound better
// under a frequency domain algorithm.
const float kMinRate = 0.75f;
const float kMaxRate = 4.0f;

AudioRendererAlgorithmOLA::AudioRendererAlgorithmOLA()
    : input_step_(0),
      output_step_(0),
      window_size_(0) {
}

AudioRendererAlgorithmOLA::~AudioRendererAlgorithmOLA() {
}

size_t AudioRendererAlgorithmOLA::FillBuffer(uint8* dest, size_t length) {
  if (IsQueueEmpty())
    return 0;
  if (playback_rate() == 0.0f)
    return 0;

  size_t dest_written = 0;

  // Handle the simple case of normal playback.
  if (playback_rate() == 1.0f) {
    if (QueueSize() < length)
      dest_written = CopyFromInput(dest, QueueSize());
    else
      dest_written = CopyFromInput(dest, length);
    AdvanceInputPosition(dest_written);
    return dest_written;
  }

  // Mute when out of acceptable quality range. Note: This may not play at the
  // speed requested as we can only consume as much data as we have, and audio
  // timestamps drive the pipeline clock.
  if (playback_rate() < kMinRate || playback_rate() > kMaxRate) {
    size_t consume = static_cast<size_t>(length * playback_rate());
    size_t safe_to_consume = std::min(QueueSize(), consume);
    memset(dest, 0, length);
    AlignToSampleBoundary(&safe_to_consume);
    AdvanceInputPosition(safe_to_consume);
    return length;
  }

  // For other playback rates, OLA with crossfade!
  while (length >= output_step_ + crossfade_size_) {
    // If we don't have enough data to completely finish this loop, quit.
    if (QueueSize() < window_size_)
      break;

    // Copy bulk of data to output (including some to crossfade to the next
    // copy), then add to our running sum of written data and subtract from
    // our tally of remaing requested.
    size_t copied = CopyFromInput(dest, output_step_ + crossfade_size_);
    dest_written += copied;
    length -= copied;

    // Advance pointers for crossfade.
    dest += output_step_;
    AdvanceInputPosition(input_step_);

    // Prepare intermediate buffer.
    size_t crossfade_size;
    scoped_array<uint8> src(new uint8[crossfade_size_]);
    crossfade_size = CopyFromInput(src.get(), crossfade_size_);

    // Calculate number of samples to crossfade, then do so.
    int samples = static_cast<int>(crossfade_size / sample_bytes()
        / channels());
    switch (sample_bytes()) {
      case 4:
        Crossfade(samples,
            reinterpret_cast<const int32*>(src.get()),
            reinterpret_cast<int32*>(dest));
        break;
      case 2:
        Crossfade(samples,
            reinterpret_cast<const int16*>(src.get()),
            reinterpret_cast<int16*>(dest));
        break;
      case 1:
        Crossfade(samples, src.get(), dest);
        break;
      default:
        NOTREACHED() << "Unsupported audio bit depth sent to OLA algorithm";
    }

    // Advance pointers again.
    AdvanceInputPosition(crossfade_size);
    dest += crossfade_size_;
  }
  return dest_written;
}

void AudioRendererAlgorithmOLA::set_playback_rate(float new_rate) {
  AudioRendererAlgorithmBase::set_playback_rate(new_rate);

  // Calculate the window size from our default length and our audio properties.
  // Precision is not an issue because we will round this to a sample boundary.
  // This will not overflow because each parameter is checked in Initialize().
  window_size_ = static_cast<size_t>(sample_rate()
                                     * sample_bytes()
                                     * channels()
                                     * kDefaultWindowLength);

  // Adjusting step sizes to accomodate requested playback rate.
  if (playback_rate() > 1.0f) {
    input_step_ = window_size_;
    output_step_ = static_cast<size_t>(ceil(
        static_cast<float>(window_size_ / playback_rate())));
  } else {
    input_step_ = static_cast<size_t>(ceil(
        static_cast<float>(window_size_ * playback_rate())));
    output_step_ = window_size_;
  }
  AlignToSampleBoundary(&input_step_);
  AlignToSampleBoundary(&output_step_);

  // Calculate length for crossfading.
  crossfade_size_ = static_cast<size_t>(sample_rate()
                                        * sample_bytes()
                                        * channels()
                                        * kDefaultCrossfadeLength);
  AlignToSampleBoundary(&crossfade_size_);

  // To keep true to playback rate, modify the steps.
  input_step_ -= crossfade_size_;
  output_step_ -= crossfade_size_;
}

void AudioRendererAlgorithmOLA::AlignToSampleBoundary(size_t* value) {
  (*value) -= ((*value) % (channels() * sample_bytes()));
}

template <class Type>
void AudioRendererAlgorithmOLA::Crossfade(int samples,
                                          const Type* src,
                                          Type* dest) {
  Type* dest_end = dest + samples * channels();
  const Type* src_end = src + samples * channels();
  for (int i = 0; i < samples; ++i) {
    double x_ratio = static_cast<double>(i) / static_cast<double>(samples);
    for (int j = 0; j < channels(); ++j) {
      DCHECK(dest < dest_end);
      DCHECK(src < src_end);
      (*dest) = static_cast<Type>((*dest) * (1.0 - x_ratio) +
                                  (*src) * x_ratio);
      ++src;
      ++dest;
    }
  }
}

}  // namespace media
