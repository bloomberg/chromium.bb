// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_hardware.h"

#include "base/logging.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

static int output_sample_rate = 0;
static int input_sample_rate = 0;
static size_t output_buffer_size = 0;
static ChannelLayout input_channel_layout = CHANNEL_LAYOUT_NONE;

namespace audio_hardware {

int GetOutputSampleRate() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!output_sample_rate) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareSampleRate(&output_sample_rate));
  }
  return output_sample_rate;
}

int GetInputSampleRate() {
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
  // TODO(vrk/crogers): The buffer sizes that this function computes is probably
  // overly conservative. However, reducing the buffer size to 2048-8192 bytes
  // caused crbug.com/108396. This computation should be revisited while making
  // sure crbug.com/108396 doesn't happen again.

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

ChannelLayout GetInputChannelLayout() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (input_channel_layout == CHANNEL_LAYOUT_NONE) {
    ChannelLayout layout = CHANNEL_LAYOUT_NONE;
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareInputChannelLayout(&layout));
    input_channel_layout = layout;
  }

  return input_channel_layout;
}

void ResetCache() {
  DCHECK(RenderThreadImpl::current() != NULL);

  output_sample_rate = 0.0;
  input_sample_rate = 0.0;
  output_buffer_size = 0;
  input_channel_layout = CHANNEL_LAYOUT_NONE;
}

}  // namespace audio_hardware
