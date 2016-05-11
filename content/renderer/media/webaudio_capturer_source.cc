// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webaudio_capturer_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/renderer/media/webrtc_local_audio_track.h"

using media::AudioBus;
using media::AudioParameters;
using media::ChannelLayout;
using media::CHANNEL_LAYOUT_MONO;
using media::CHANNEL_LAYOUT_STEREO;

namespace content {

WebAudioCapturerSource::WebAudioCapturerSource(
    blink::WebMediaStreamSource* blink_source)
    : track_(NULL),
      audio_format_changed_(false),
      fifo_(base::Bind(&WebAudioCapturerSource::DeliverRebufferedAudio,
                       base::Unretained(this))),
      blink_source_(*blink_source) {
  DCHECK(blink_source);
  DCHECK(!blink_source_.isNull());
  DVLOG(1) << "WebAudioCapturerSource::WebAudioCapturerSource()";
  blink_source_.addAudioConsumer(this);
}

WebAudioCapturerSource::~WebAudioCapturerSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebAudioCapturerSource::~WebAudioCapturerSource()";
  DeregisterFromBlinkSource();
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
  fifo_.Reset(sample_rate / 100);
  params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                sample_rate, 16, fifo_.frames_per_buffer());

  // Take care of the discrete channel layout case.
  params_.set_channels_for_discrete(number_of_channels);

  audio_format_changed_ = true;

  if (!wrapper_bus_ ||
      wrapper_bus_->channels() != static_cast<int>(number_of_channels)) {
    wrapper_bus_ = AudioBus::CreateWrapper(params_.channels());
  }
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
  // DeregisterFromBlinkSource() should not be called while |lock_| is acquired,
  // as it could result in a deadlock.
  DeregisterFromBlinkSource();
}

void WebAudioCapturerSource::consumeAudio(
    const blink::WebVector<const float*>& audio_data,
    size_t number_of_frames) {
  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync.  http://crbug.com/335335
  current_reference_time_ = base::TimeTicks::Now();

  base::AutoLock auto_lock(lock_);
  if (!track_)
    return;

  // Update the downstream client if the audio format has been changed.
  if (audio_format_changed_) {
    track_->OnSetFormat(params_);
    audio_format_changed_ = false;
  }

  wrapper_bus_->set_frames(number_of_frames);
  DCHECK_EQ(params_.channels(), static_cast<int>(audio_data.size()));
  for (size_t i = 0; i < audio_data.size(); ++i)
    wrapper_bus_->SetChannelData(i, const_cast<float*>(audio_data[i]));

  // The following will result in zero, one, or multiple synchronous calls to
  // DeliverRebufferedAudio().
  fifo_.Push(*wrapper_bus_);
}

void WebAudioCapturerSource::DeliverRebufferedAudio(
    const media::AudioBus& audio_bus,
    int frame_delay) {
  lock_.AssertAcquired();
  const base::TimeTicks reference_time =
      current_reference_time_ +
      base::TimeDelta::FromMicroseconds(frame_delay *
                                        base::Time::kMicrosecondsPerSecond /
                                        params_.sample_rate());
  track_->Capture(audio_bus, reference_time);
}

void WebAudioCapturerSource::DeregisterFromBlinkSource() {
  if (!blink_source_.isNull()) {
    blink_source_.removeAudioConsumer(this);
    blink_source_.reset();
  }
}

}  // namespace content
