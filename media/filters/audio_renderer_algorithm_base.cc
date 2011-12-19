// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

// The size in bytes we try to maintain for the |queue_|. Previous usage
// maintained a deque of 16 Buffers, each of size 4Kb. This worked well, so we
// maintain this number of bytes (16 * 4096).
static const int kDefaultMinQueueSizeInBytes = 65536;
// ~64kb @ 44.1k stereo.
static const int kDefaultMinQueueSizeInMilliseconds = 372;
// 3 seconds @ 96kHz 7.1.
static const int kDefaultMaxQueueSizeInBytes = 4608000;
static const int kDefaultMaxQueueSizeInMilliseconds = 3000;

// Default window and crossfade lengths in seconds.
static const double kDefaultWindowLength = 0.08;
static const double kDefaultCrossfadeLength = 0.008;

// Default mute ranges for fast/slow audio. These rates would sound better
// under a frequency domain algorithm.
static const float kMinRate = 0.5f;
static const float kMaxRate = 4.0f;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      sample_rate_(0),
      sample_bytes_(0),
      playback_rate_(0.0f),
      queue_(0, kDefaultMinQueueSizeInBytes),
      max_queue_capacity_(kDefaultMaxQueueSizeInBytes),
      input_step_(0),
      output_step_(0),
      crossfade_size_(0),
      window_size_(0) {
}

uint32 AudioRendererAlgorithmBase::FillBuffer(uint8* dest, uint32 length) {
  if (IsQueueEmpty())
    return 0;
  if (playback_rate_ == 0.0f)
    return 0;

  uint32 dest_written = 0;

  // Handle the simple case of normal playback.
  if (playback_rate_ == 1.0f) {
    if (QueueSize() < length)
      dest_written = CopyFromInput(dest, QueueSize());
    else
      dest_written = CopyFromInput(dest, length);
    AdvanceInputPosition(dest_written);
    return dest_written;
  }

  // For other playback rates, OLA with crossfade!
  while (true) {
    // Mute when out of acceptable quality range or when we don't have enough
    // data to completely finish this loop.
    //
    // Note: This may not play at the speed requested as we can only consume as
    // much data as we have, and audio timestamps drive the pipeline clock.
    //
    // Furthermore, we won't end up scaling the very last bit of audio, but
    // we're talking about <8ms of audio data.
    if (playback_rate_ < kMinRate || playback_rate_ > kMaxRate ||
        QueueSize() < window_size_) {
      // Calculate the ideal input/output steps based on the size of the
      // destination buffer.
      uint32 input_step = static_cast<uint32>(ceil(
          static_cast<float>(length * playback_rate_)));
      uint32 output_step = length;

      // If the ideal size is too big, recalculate based on how much is left in
      // the queue.
      if (input_step > QueueSize()) {
        input_step = QueueSize();
        output_step = static_cast<uint32>(ceil(
            static_cast<float>(input_step / playback_rate_)));
      }

      // Stay aligned and sanity check before writing out zeros.
      AlignToSampleBoundary(&input_step);
      AlignToSampleBoundary(&output_step);
      DCHECK_LE(output_step, length);
      if (output_step > length) {
        LOG(ERROR) << "OLA: output_step (" << output_step << ") calculated to "
                   << "be larger than destination length (" << length << ")";
        output_step = length;
      }

      memset(dest, 0, output_step);
      AdvanceInputPosition(input_step);
      dest_written += output_step;
      break;
    }

    // Break if we don't have enough room left in our buffer to do a full
    // OLA iteration.
    if (length < (output_step_ + crossfade_size_)) {
      break;
    }

    // Copy bulk of data to output (including some to crossfade to the next
    // copy), then add to our running sum of written data and subtract from
    // our tally of remaining requested.
    uint32 copied = CopyFromInput(dest, output_step_ + crossfade_size_);
    dest_written += copied;
    length -= copied;

    // Advance pointers for crossfade.
    dest += output_step_;
    AdvanceInputPosition(input_step_);

    // Prepare intermediate buffer.
    uint32 crossfade_size;
    scoped_array<uint8> src(new uint8[crossfade_size_]);
    crossfade_size = CopyFromInput(src.get(), crossfade_size_);

    // Calculate number of samples to crossfade, then do so.
    int samples = static_cast<int>(crossfade_size / sample_bytes_ / channels_);
    switch (sample_bytes_) {
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

void AudioRendererAlgorithmBase::SetPlaybackRate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;

  // Calculate the window size from our default length and our audio properties.
  // Precision is not an issue because we will round this to a sample boundary.
  // This will not overflow because each parameter is checked in Initialize().
  window_size_ = static_cast<uint32>(
      sample_rate_ * sample_bytes_ * channels_ * kDefaultWindowLength);

  // Adjusting step sizes to accommodate requested playback rate.
  if (playback_rate_ > 1.0f) {
    input_step_ = window_size_;
    output_step_ = static_cast<uint32>(ceil(
        static_cast<float>(window_size_ / playback_rate_)));
  } else {
    input_step_ = static_cast<uint32>(ceil(
        static_cast<float>(window_size_ * playback_rate_)));
    output_step_ = window_size_;
  }
  AlignToSampleBoundary(&input_step_);
  AlignToSampleBoundary(&output_step_);

  // Calculate length for crossfading.
  crossfade_size_ = static_cast<uint32>(
      sample_rate_ * sample_bytes_ * channels_ * kDefaultCrossfadeLength);
  AlignToSampleBoundary(&crossfade_size_);
  if (crossfade_size_ > std::min(input_step_, output_step_)) {
    crossfade_size_ = 0;
    return;
  }

  // To keep true to playback rate, modify the steps.
  input_step_ -= crossfade_size_;
  output_step_ -= crossfade_size_;
}

void AudioRendererAlgorithmBase::AlignToSampleBoundary(uint32* value) {
  (*value) -= ((*value) % (channels_ * sample_bytes_));
}

template <class Type>
void AudioRendererAlgorithmBase::Crossfade(int samples, const Type* src,
                                           Type* dest) {
  Type* dest_end = dest + samples * channels_;
  const Type* src_end = src + samples * channels_;
  for (int i = 0; i < samples; ++i) {
    double x_ratio = static_cast<double>(i) / static_cast<double>(samples);
    for (int j = 0; j < channels_; ++j) {
      DCHECK(dest < dest_end);
      DCHECK(src < src_end);
      (*dest) = static_cast<Type>((*dest) * (1.0 - x_ratio) +
                                  (*src) * x_ratio);
      ++src;
      ++dest;
    }
  }
}

AudioRendererAlgorithmBase::~AudioRendererAlgorithmBase() {}

void AudioRendererAlgorithmBase::Initialize(
    int channels,
    int sample_rate,
    int sample_bits,
    float initial_playback_rate,
    const base::Closure& callback) {
  DCHECK_GT(channels, 0);
  DCHECK_LE(channels, 8) << "We only support <= 8 channel audio.";
  DCHECK_GT(sample_rate, 0);
  DCHECK_LE(sample_rate, 256000)
      << "We only support sample rates at or below 256000Hz.";
  DCHECK_GT(sample_bits, 0);
  DCHECK_LE(sample_bits, 32) << "We only support 8, 16, 32 bit audio.";
  DCHECK_EQ(sample_bits % 8, 0) << "We only support 8, 16, 32 bit audio.";
  DCHECK(!callback.is_null());

  channels_ = channels;
  sample_rate_ = sample_rate;
  sample_bytes_ = sample_bits / 8;

  // Update the capacity based on time now that we have the audio format
  // parameters.
  queue_.set_forward_capacity(
      DurationToBytes(kDefaultMinQueueSizeInMilliseconds));
  max_queue_capacity_ =
      std::min(kDefaultMaxQueueSizeInBytes,
               DurationToBytes(kDefaultMaxQueueSizeInMilliseconds));

  request_read_callback_ = callback;

  SetPlaybackRate(initial_playback_rate);
}

void AudioRendererAlgorithmBase::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  queue_.Clear();
  request_read_callback_.Run();
}

base::TimeDelta AudioRendererAlgorithmBase::GetTime() {
  return queue_.current_time();
}

void AudioRendererAlgorithmBase::EnqueueBuffer(Buffer* buffer_in) {
  // If we're at end of stream, |buffer_in| contains no data.
  if (!buffer_in->IsEndOfStream())
    queue_.Append(buffer_in);

  // If we still don't have enough data, request more.
  if (!IsQueueFull())
    request_read_callback_.Run();
}

float AudioRendererAlgorithmBase::playback_rate() {
  return playback_rate_;
}

bool AudioRendererAlgorithmBase::IsQueueEmpty() {
  return queue_.forward_bytes() == 0;
}

bool AudioRendererAlgorithmBase::IsQueueFull() {
  return (queue_.forward_bytes() >= queue_.forward_capacity());
}

uint32 AudioRendererAlgorithmBase::QueueSize() {
  return queue_.forward_bytes();
}

void AudioRendererAlgorithmBase::IncreaseQueueCapacity() {
  queue_.set_forward_capacity(
      std::min(2 * queue_.forward_capacity(), max_queue_capacity_));
}

int AudioRendererAlgorithmBase::DurationToBytes(
    int duration_in_milliseconds) const {
  int64 bytes_per_second = sample_bytes_ * channels_ * sample_rate_;
  int64 bytes = duration_in_milliseconds * bytes_per_second / 1000;
  return std::min(bytes, static_cast<int64>(kint32max));
}

void AudioRendererAlgorithmBase::AdvanceInputPosition(uint32 bytes) {
  queue_.Seek(bytes);

  if (!IsQueueFull())
    request_read_callback_.Run();
}

uint32 AudioRendererAlgorithmBase::CopyFromInput(uint8* dest, uint32 bytes) {
  return queue_.Peek(dest, bytes);
}

}  // namespace media
