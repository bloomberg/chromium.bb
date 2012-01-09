// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>

#include "base/bind.h"
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
      thread_("AudioThread") {
}

NullAudioRenderer::~NullAudioRenderer() {
  DCHECK(!thread_.IsRunning());
}

void NullAudioRenderer::SetVolume(float volume) {
  // Do nothing.
}

bool NullAudioRenderer::OnInitialize(int bits_per_channel,
                                     ChannelLayout channel_layout,
                                     int sample_rate) {
  // Calculate our bytes per millisecond value and allocate our buffer.
  bytes_per_millisecond_ =
      (ChannelLayoutToChannelCount(channel_layout) * sample_rate *
       bits_per_channel / 8) / base::Time::kMillisecondsPerSecond;
  buffer_size_ = bytes_per_millisecond_ * kBufferSizeInMilliseconds;
  buffer_.reset(new uint8[buffer_size_]);
  DCHECK(buffer_.get());

  if (!thread_.Start())
    return false;

  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &NullAudioRenderer::FillBufferTask, this));
  return true;
}

void NullAudioRenderer::OnStop() {
  thread_.Stop();
}

void NullAudioRenderer::FillBufferTask() {
  int64 sleep_in_milliseconds = 0;

  // Only consume buffers when actually playing.
  if (GetPlaybackRate() > 0.0f)  {
    size_t bytes = FillBuffer(buffer_.get(),
                              buffer_size_,
                              base::TimeDelta(),
                              true);

    // Calculate our sleep duration, taking playback rate into consideration.
    sleep_in_milliseconds =
        bytes / (bytes_per_millisecond_ * GetPlaybackRate());
  } else {
    // If paused, sleep for 10 milliseconds before polling again.
    sleep_in_milliseconds = 10;
  }

  // Sleep for at least one millisecond so we don't spin the CPU.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NullAudioRenderer::FillBufferTask, this),
      std::max(sleep_in_milliseconds, static_cast<int64>(1)));
}

}  // namespace media
