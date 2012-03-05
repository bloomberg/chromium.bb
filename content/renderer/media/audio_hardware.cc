// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_hardware.h"

#include "base/logging.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

static double output_sample_rate = 0.0;
static double input_sample_rate = 0.0;
static size_t output_buffer_size = 0;
static uint32 input_channel_count = 0;

namespace audio_hardware {

double GetOutputSampleRate() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!output_sample_rate) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareSampleRate(&output_sample_rate));
  }
  return output_sample_rate;
}

double GetInputSampleRate() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!input_sample_rate) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareInputSampleRate(&input_sample_rate));
  }
  return input_sample_rate;
}

size_t GetOutputBufferSize() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!output_buffer_size) {
    uint32 buffer_size = 0;
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareBufferSize(&buffer_size));
    output_buffer_size = buffer_size;
  }

  return output_buffer_size;
}

size_t GetHighLatencyOutputBufferSize(int sample_rate) {
  // The minimum number of samples in a hardware packet.
  // This value is selected so that we can handle down to 5khz sample rate.
  static const size_t kMinSamplesPerHardwarePacket = 1024;

  // The maximum number of samples in a hardware packet.
  // This value is selected so that we can handle up to 192khz sample rate.
  static const size_t kMaxSamplesPerHardwarePacket = 64 * 1024;

  // This constant governs the hardware audio buffer size, this value should be
  // chosen carefully.
  // This value is selected so that we have 8192 samples for 48khz streams.
  static const size_t kMillisecondsPerHardwarePacket = 170;

  // Select the number of samples that can provide at least
  // |kMillisecondsPerHardwarePacket| worth of audio data.
  size_t samples = kMinSamplesPerHardwarePacket;
  while (samples <= kMaxSamplesPerHardwarePacket &&
         samples * base::Time::kMillisecondsPerSecond <
         sample_rate * kMillisecondsPerHardwarePacket) {
    samples *= 2;
  }
  return samples;
}

uint32 GetInputChannelCount() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!input_channel_count) {
    uint32 channels = 0;
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareInputChannelCount(&channels));
    input_channel_count = channels;
  }

  return input_channel_count;
}

void ResetCache() {
  DCHECK(RenderThreadImpl::current() != NULL);

  output_sample_rate = 0.0;
  input_sample_rate = 0.0;
  output_buffer_size = 0;
  input_channel_count = 0;
}

}  // namespace audio_hardware
