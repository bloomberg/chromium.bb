// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

// The size in bytes we try to maintain for the |queue_|. Previous usage
// maintained a deque of 16 Buffers, each of size 4Kb. This worked well, so we
// maintain this number of bytes (16 * 4096).
const int kDefaultMinQueueSizeInBytes = 65536;
const int kDefaultMinQueueSizeInMilliseconds = 372; // ~64kb @ 44.1k stereo
const int kDefaultMaxQueueSizeInBytes = 4608000; // 3 seconds @ 96kHz 7.1
const int kDefaultMaxQueueSizeInMilliseconds = 3000;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      sample_rate_(0),
      sample_bytes_(0),
      playback_rate_(0.0f),
      queue_(0, kDefaultMinQueueSizeInBytes),
      max_queue_capacity_(kDefaultMaxQueueSizeInBytes) {
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

  set_playback_rate(initial_playback_rate);
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

void AudioRendererAlgorithmBase::set_playback_rate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;
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

int AudioRendererAlgorithmBase::channels() {
  return channels_;
}

int AudioRendererAlgorithmBase::sample_rate() {
  return sample_rate_;
}

int AudioRendererAlgorithmBase::sample_bytes() {
  return sample_bytes_;
}

}  // namespace media
