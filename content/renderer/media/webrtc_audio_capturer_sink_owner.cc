// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer_sink_owner.h"

namespace content {

WebRtcAudioCapturerSinkOwner::WebRtcAudioCapturerSinkOwner(
    WebRtcAudioCapturerSink* sink)
    : delegate_(sink) {
}

void WebRtcAudioCapturerSinkOwner::CaptureData(
    const int16* audio_data, int number_of_channels, int number_of_frames,
    int audio_delay_milliseconds, double volume) {
  base::AutoLock lock(lock_);
  if (delegate_) {
    delegate_->CaptureData(audio_data, number_of_channels, number_of_frames,
                           audio_delay_milliseconds, volume);
  }
}

void WebRtcAudioCapturerSinkOwner::SetCaptureFormat(
    const media::AudioParameters& params) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->SetCaptureFormat(params);
}

bool WebRtcAudioCapturerSinkOwner::IsEqual(
    const WebRtcAudioCapturerSink* other) const {
  base::AutoLock lock(lock_);
  return (other == delegate_);
}

void WebRtcAudioCapturerSinkOwner::Reset() {
  base::AutoLock lock(lock_);
  delegate_ = NULL;
}

}  // namespace content
