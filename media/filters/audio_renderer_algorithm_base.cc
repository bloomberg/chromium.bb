// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_util.h"
#include "media/base/buffers.h"

namespace media {

// The starting size in bytes for |audio_buffer_|.
// Previous usage maintained a deque of 16 Buffers, each of size 4Kb. This
// worked well, so we maintain this number of bytes (16 * 4096).
static const size_t kStartingBufferSizeInBytes = 65536;

// The maximum size in bytes for the |audio_buffer_|. Arbitrarily determined.
// This number represents 3 seconds of 96kHz/16 bit 7.1 surround sound.
static const size_t kMaxBufferSizeInBytes = 4608000;

// Duration of audio segments used for crossfading (in seconds).
static const double kWindowDuration = 0.08;

// Duration of crossfade between audio segments (in seconds).
static const double kCrossfadeDuration = 0.008;

// Max/min supported playback rates for fast/slow audio. Audio outside of these
// ranges are muted.
// Audio at these speeds would sound better under a frequency domain algorithm.
static const float kMinPlaybackRate = 0.5f;
static const float kMaxPlaybackRate = 4.0f;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      samples_per_second_(0),
      bytes_per_channel_(0),
      playback_rate_(0.0f),
      audio_buffer_(0, kStartingBufferSizeInBytes),
      crossfade_size_(0),
      window_size_(0) {
}

AudioRendererAlgorithmBase::~AudioRendererAlgorithmBase() {}

void AudioRendererAlgorithmBase::Initialize(
    int channels,
    int samples_per_second,
    int bits_per_channel,
    float initial_playback_rate,
    const base::Closure& callback) {
  DCHECK_GT(channels, 0);
  DCHECK_LE(channels, 8) << "We only support <= 8 channel audio.";
  DCHECK_GT(samples_per_second, 0);
  DCHECK_LE(samples_per_second, 256000)
      << "We only support sample rates at or below 256000Hz.";
  DCHECK_GT(bits_per_channel, 0);
  DCHECK_LE(bits_per_channel, 32) << "We only support 8, 16, 32 bit audio.";
  DCHECK_EQ(bits_per_channel % 8, 0) << "We only support 8, 16, 32 bit audio.";
  DCHECK(!callback.is_null());

  channels_ = channels;
  samples_per_second_ = samples_per_second;
  bytes_per_channel_ = bits_per_channel / 8;
  request_read_cb_ = callback;
  SetPlaybackRate(initial_playback_rate);

  window_size_ =
      samples_per_second_ * bytes_per_channel_ * channels_ * kWindowDuration;
  AlignToSampleBoundary(&window_size_);

  crossfade_size_ =
      samples_per_second_ * bytes_per_channel_ * channels_ * kCrossfadeDuration;
  AlignToSampleBoundary(&crossfade_size_);
}

uint32 AudioRendererAlgorithmBase::FillBuffer(uint8* dest, uint32 length) {
  if (IsQueueEmpty() || playback_rate_ == 0.0f)
    return 0;

  // Handle the simple case of normal playback.
  if (playback_rate_ == 1.0f) {
    uint32 bytes_written =
        CopyFromAudioBuffer(dest, std::min(length, bytes_buffered()));
    AdvanceBufferPosition(bytes_written);
    return bytes_written;
  }

  // Output muted data when out of acceptable quality range.
  if (playback_rate_ < kMinPlaybackRate || playback_rate_ > kMaxPlaybackRate)
    return MuteBuffer(dest, length);

  uint32 input_step = window_size_;
  uint32 output_step = window_size_;

  if (playback_rate_ > 1.0f) {
    // Playback is faster than normal; need to squish output!
    output_step = ceil(window_size_ / playback_rate_);
  } else {
    // Playback is slower than normal; need to stretch input!
    input_step = ceil(window_size_ * playback_rate_);
  }

  AlignToSampleBoundary(&input_step);
  AlignToSampleBoundary(&output_step);
  DCHECK_LE(crossfade_size_, input_step);
  DCHECK_LE(crossfade_size_, output_step);

  uint32 bytes_written = 0;
  uint32 bytes_left_to_output = length;
  uint8* output_ptr = dest;

  // TODO(vrk): The while loop and if test below are lame! We are requiring the
  // client to provide us with enough data to output only complete crossfaded
  // windows. Instead, we should output as much data as we can, and add state to
  // keep track of what point in the crossfade we are at.
  // This is also the cause of crbug.com/108239.
  while (bytes_left_to_output >= output_step) {
    // If there is not enough data buffered to complete an iteration of the
    // loop, mute the remaining and break.
    if (bytes_buffered() < window_size_) {
      bytes_written += MuteBuffer(output_ptr, bytes_left_to_output);
      break;
    }

    // Copy |output_step| bytes into destination buffer.
    uint32 copied = CopyFromAudioBuffer(output_ptr, output_step);
    DCHECK_EQ(copied, output_step);
    output_ptr += output_step;
    bytes_written += copied;
    bytes_left_to_output -= copied;

    // Copy the |crossfade_size_| bytes leading up to the next window that will
    // be played into an intermediate buffer. This will be used to crossfade
    // from the current window to the next.
    AdvanceBufferPosition(input_step - crossfade_size_);
    scoped_array<uint8> next_window_intro(new uint8[crossfade_size_]);
    uint32 bytes_copied =
        CopyFromAudioBuffer(next_window_intro.get(), crossfade_size_);
    DCHECK_EQ(bytes_copied, crossfade_size_);
    AdvanceBufferPosition(crossfade_size_);

    // Prepare pointers to end of the current window and the start of the next
    // window.
    uint8* start_of_outro = output_ptr - crossfade_size_;
    const uint8* start_of_intro = next_window_intro.get();

    // Do crossfade!
    Crossfade(crossfade_size_, channels_, bytes_per_channel_,
              start_of_intro, start_of_outro);
  }

  return bytes_written;
}

uint32 AudioRendererAlgorithmBase::MuteBuffer(uint8* dest, uint32 length) {
  DCHECK_NE(playback_rate_, 0.0);
  // Note: This may not play at the speed requested as we can only consume as
  // much data as we have, and audio timestamps drive the pipeline clock.
  //
  // Furthermore, we won't end up scaling the very last bit of audio, but
  // we're talking about <8ms of audio data.

  // Cap the |input_step| by the amount of bytes buffered.
  uint32 input_step =
      std::min(static_cast<uint32>(length * playback_rate_), bytes_buffered());
  uint32 output_step = input_step / playback_rate_;
  AlignToSampleBoundary(&input_step);
  AlignToSampleBoundary(&output_step);

  DCHECK_LE(output_step, length);
  if (output_step > length) {
    LOG(ERROR) << "OLA: output_step (" << output_step << ") calculated to "
               << "be larger than destination length (" << length << ")";
    output_step = length;
  }

  memset(dest, 0, output_step);
  AdvanceBufferPosition(input_step);
  return output_step;
}

void AudioRendererAlgorithmBase::SetPlaybackRate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;
}

void AudioRendererAlgorithmBase::AlignToSampleBoundary(uint32* value) {
  (*value) -= ((*value) % (channels_ * bytes_per_channel_));
}

void AudioRendererAlgorithmBase::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  audio_buffer_.Clear();
  request_read_cb_.Run();
}

base::TimeDelta AudioRendererAlgorithmBase::GetTime() {
  return audio_buffer_.current_time();
}

void AudioRendererAlgorithmBase::EnqueueBuffer(Buffer* buffer_in) {
  // If we're at end of stream, |buffer_in| contains no data.
  if (!buffer_in->IsEndOfStream())
    audio_buffer_.Append(buffer_in);

  // If we still don't have enough data, request more.
  if (!IsQueueFull())
    request_read_cb_.Run();
}

bool AudioRendererAlgorithmBase::IsQueueEmpty() {
  return audio_buffer_.forward_bytes() == 0;
}

bool AudioRendererAlgorithmBase::IsQueueFull() {
  return audio_buffer_.forward_bytes() >= audio_buffer_.forward_capacity();
}

uint32 AudioRendererAlgorithmBase::QueueCapacity() {
  return audio_buffer_.forward_capacity();
}

void AudioRendererAlgorithmBase::IncreaseQueueCapacity() {
  audio_buffer_.set_forward_capacity(
      std::min(2 * audio_buffer_.forward_capacity(), kMaxBufferSizeInBytes));
}

void AudioRendererAlgorithmBase::AdvanceBufferPosition(uint32 bytes) {
  audio_buffer_.Seek(bytes);

  if (!IsQueueFull())
    request_read_cb_.Run();
}

uint32 AudioRendererAlgorithmBase::CopyFromAudioBuffer(
    uint8* dest, uint32 bytes) {
  return audio_buffer_.Peek(dest, bytes);
}

}  // namespace media
