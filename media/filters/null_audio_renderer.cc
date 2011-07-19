// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "media/base/filter_host.h"
#include "media/filters/null_audio_renderer.h"

namespace media {

// How "long" our buffer should be in terms of milliseconds.  In OnInitialize
// we calculate the size of one second of audio data and use this number to
// allocate a buffer to pass to FillBuffer.
static const size_t kBufferSizeInMilliseconds = 100;

NullAudioRenderer::NullAudioRenderer()
    : AudioRendererBase(),
      bytes_per_millisecond_(0),
      buffer_size_(0),
      thread_(base::kNullThreadHandle),
      shutdown_(false) {
}

NullAudioRenderer::~NullAudioRenderer() {
  DCHECK_EQ(base::kNullThreadHandle, thread_);
}

void NullAudioRenderer::SetVolume(float volume) {
  // Do nothing.
}

void NullAudioRenderer::ThreadMain() {
  // Loop until we're signaled to stop.
  while (!shutdown_) {
    float sleep_in_milliseconds = 0.0f;

    // Only consume buffers when actually playing.
    if (GetPlaybackRate() > 0.0f)  {
      size_t bytes = FillBuffer(buffer_.get(),
                                buffer_size_,
                                base::TimeDelta(),
                                true);

      // Calculate our sleep duration, taking playback rate into consideration.
      sleep_in_milliseconds =
          floor(bytes / static_cast<float>(bytes_per_millisecond_));
      sleep_in_milliseconds /= GetPlaybackRate();
    } else {
      // If paused, sleep for 10 milliseconds before polling again.
      sleep_in_milliseconds = 10.0f;
    }

    // Sleep for at least one millisecond so we don't spin the CPU.
    base::PlatformThread::Sleep(
        std::max(1, static_cast<int>(sleep_in_milliseconds)));
  }
}

bool NullAudioRenderer::OnInitialize(const AudioDecoderConfig& config) {
  // Calculate our bytes per millisecond value and allocate our buffer.
  bytes_per_millisecond_ =
      (ChannelLayoutToChannelCount(config.channel_layout) * config.sample_rate *
       config.bits_per_channel / 8) / base::Time::kMillisecondsPerSecond;
  buffer_size_ = bytes_per_millisecond_ * kBufferSizeInMilliseconds;
  buffer_.reset(new uint8[buffer_size_]);
  DCHECK(buffer_.get());

  // It's safe to start the thread now because it simply sleeps when playback
  // rate is 0.0f.
  return base::PlatformThread::Create(0, this, &thread_);
}

void NullAudioRenderer::OnStop() {
  shutdown_ = true;
  if (thread_) {
    base::PlatformThread::Join(thread_);
    thread_ = base::kNullThreadHandle;
  }
}

}  // namespace media
