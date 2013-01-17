// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webaudio_capturer_source.h"

#include "base/logging.h"
#include "content/renderer/media/webrtc_audio_capturer.h"

using media::AudioBus;
using media::AudioFifo;
using media::AudioParameters;
using media::ChannelLayout;
using media::CHANNEL_LAYOUT_MONO;
using media::CHANNEL_LAYOUT_STEREO;

static const int kFifoSize = 2048;

namespace content {

WebAudioCapturerSource::WebAudioCapturerSource(WebRtcAudioCapturer* capturer)
    : capturer_(capturer),
      set_format_channels_(0),
      callback_(0),
      started_(false) {
}

WebAudioCapturerSource::~WebAudioCapturerSource() {
}

void WebAudioCapturerSource::setFormat(
    size_t number_of_channels, float sample_rate) {
  if (number_of_channels <= 2) {
    set_format_channels_ = number_of_channels;
    ChannelLayout channel_layout =
        number_of_channels == 1 ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
    capturer_->SetCapturerSource(this, channel_layout, sample_rate);
    capturer_->Start();
  } else {
    // TODO(crogers): Handle more than just the mono and stereo cases.
    LOG(WARNING) << "WebAudioCapturerSource::setFormat() : unhandled format.";
  }
}

void WebAudioCapturerSource::Initialize(
    const media::AudioParameters& params,
    media::AudioCapturerSource::CaptureCallback* callback,
    media::AudioCapturerSource::CaptureEventHandler* event_handler) {
  // The downstream client should be configured the same as what WebKit
  // is feeding it.
  DCHECK_EQ(set_format_channels_, params.channels());

  base::AutoLock auto_lock(lock_);
  params_ = params;
  callback_ = callback;
  wrapper_bus_ = AudioBus::CreateWrapper(params.channels());
  capture_bus_ = AudioBus::Create(params);
  fifo_.reset(new AudioFifo(params.channels(), kFifoSize));
}

void WebAudioCapturerSource::Start() {
  started_ = true;
}

void WebAudioCapturerSource::Stop() {
  started_ = false;
}

void WebAudioCapturerSource::consumeAudio(
    const WebKit::WebVector<const float*>& audio_data,
    size_t number_of_frames) {
  base::AutoLock auto_lock(lock_);

  if (!callback_)
    return;

  wrapper_bus_->set_frames(number_of_frames);

  // Make sure WebKit is honoring what it told us up front
  // about the channels.
  DCHECK_EQ(set_format_channels_, static_cast<int>(audio_data.size()));
  DCHECK_EQ(set_format_channels_, wrapper_bus_->channels());

  for (size_t i = 0; i < audio_data.size(); ++i)
    wrapper_bus_->SetChannelData(i, const_cast<float*>(audio_data[i]));

  // Handle mismatch between WebAudio buffer-size and WebRTC.
  int available = fifo_->max_frames() - fifo_->frames();
  if (available < static_cast<int>(number_of_frames)) {
    LOG(ERROR) << "WebAudioCapturerSource::Consume() : FIFO overrun.";
    return;
  }

  fifo_->Push(wrapper_bus_.get());
  int capture_frames = params_.frames_per_buffer();
  while (fifo_->frames() >= capture_frames) {
    fifo_->Consume(capture_bus_.get(), 0, capture_frames);
    callback_->Capture(capture_bus_.get(), 0, 1.0);
  }
}

}  // namespace content
