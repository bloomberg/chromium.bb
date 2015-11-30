// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webaudio_capturer_source.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "content/renderer/media/webrtc_local_audio_track.h"

using media::AudioBus;
using media::AudioFifo;
using media::AudioParameters;
using media::ChannelLayout;
using media::CHANNEL_LAYOUT_MONO;
using media::CHANNEL_LAYOUT_STEREO;

static const int kMaxNumberOfBuffersInFifo = 5;

namespace content {

WebAudioCapturerSource::WebAudioCapturerSource(
    const blink::WebMediaStreamSource& blink_source)
    : track_(NULL),
      audio_format_changed_(false),
      blink_source_(blink_source) {
}

WebAudioCapturerSource::~WebAudioCapturerSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  removeFromBlinkSource();
}

void WebAudioCapturerSource::setFormat(
    size_t number_of_channels, float sample_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebAudioCapturerSource::setFormat(sample_rate="
           << sample_rate << ")";

  // If the channel count is greater than 8, use discrete layout. However,
  // anything beyond 8 is ignored by the subsequent (WebRTC) audio pipeline.
  ChannelLayout channel_layout =
      number_of_channels > 8 ? media::CHANNEL_LAYOUT_DISCRETE
                             : media::GuessChannelLayout(number_of_channels);

  base::AutoLock auto_lock(lock_);

  // Set the format used by this WebAudioCapturerSource. We are using 10ms data
  // as buffer size since that is the native buffer size of WebRtc packet
  // running on.
  params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                sample_rate, 16, sample_rate / 100);

  // Take care of the discrete channel layout case.
  params_.set_channels_for_discrete(number_of_channels);

  audio_format_changed_ = true;

  wrapper_bus_ = AudioBus::CreateWrapper(params_.channels());
  capture_bus_ = AudioBus::Create(params_);

  fifo_.reset(new AudioFifo(
      params_.channels(),
      kMaxNumberOfBuffersInFifo * params_.frames_per_buffer()));
}

void WebAudioCapturerSource::Start(WebRtcLocalAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(track);
  base::AutoLock auto_lock(lock_);
  track_ = track;
}

void WebAudioCapturerSource::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock auto_lock(lock_);
    track_ = NULL;
  }
  // removeFromBlinkSource() should not be called while |lock_| is acquired,
  // as it could result in a deadlock.
  removeFromBlinkSource();
}

void WebAudioCapturerSource::consumeAudio(
    const blink::WebVector<const float*>& audio_data,
    size_t number_of_frames) {
  base::AutoLock auto_lock(lock_);
  if (!track_)
    return;

  // Update the downstream client if the audio format has been changed.
  if (audio_format_changed_) {
    track_->OnSetFormat(params_);
    audio_format_changed_ = false;
  }

  wrapper_bus_->set_frames(number_of_frames);

  // Make sure WebKit is honoring what it told us up front
  // about the channels.
  DCHECK_EQ(params_.channels(), static_cast<int>(audio_data.size()));

  for (size_t i = 0; i < audio_data.size(); ++i)
    wrapper_bus_->SetChannelData(i, const_cast<float*>(audio_data[i]));

  // Handle mismatch between WebAudio buffer-size and WebRTC.
  int available = fifo_->max_frames() - fifo_->frames();
  if (available < static_cast<int>(number_of_frames)) {
    NOTREACHED() << "WebAudioCapturerSource::Consume() : FIFO overrun.";
    return;
  }

  // Compute the estimated capture time of the first sample frame of audio that
  // will be consumed from the FIFO in the loop below.
  base::TimeTicks estimated_capture_time = base::TimeTicks::Now() -
      fifo_->frames() * base::TimeDelta::FromSeconds(1) / params_.sample_rate();

  fifo_->Push(wrapper_bus_.get());
  while (fifo_->frames() >= capture_bus_->frames()) {
    fifo_->Consume(capture_bus_.get(), 0, capture_bus_->frames());
    track_->Capture(*capture_bus_, estimated_capture_time, false);

    // Advance the estimated capture time for the next FIFO consume operation.
    estimated_capture_time +=
        capture_bus_->frames() * base::TimeDelta::FromSeconds(1) /
            params_.sample_rate();
  }
}

// If registered as audio consumer in |blink_source_|, deregister from
// |blink_source_| and stop keeping a reference to |blink_source_|.
// Failure to call this method when stopping the track might leave an invalid
// WebAudioCapturerSource reference still registered as an audio consumer on
// the blink side.
void WebAudioCapturerSource::removeFromBlinkSource() {
  if (!blink_source_.isNull()) {
    blink_source_.removeAudioConsumer(this);
    blink_source_.reset();
  }
}

}  // namespace content
