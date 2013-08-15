// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer_sink_owner.h"

namespace content {

WebRtcAudioCapturerSinkOwner::WebRtcAudioCapturerSinkOwner(
    WebRtcAudioCapturerSink* sink)
    : delegate_(sink) {
}

int WebRtcAudioCapturerSinkOwner::CaptureData(const std::vector<int>& channels,
                                              const int16* audio_data,
                                              int sample_rate,
                                              int number_of_channels,
                                              int number_of_frames,
                                              int audio_delay_milliseconds,
                                              int current_volume,
                                              bool need_audio_processing) {
  base::AutoLock lock(lock_);
  if (delegate_) {
    return delegate_->CaptureData(channels, audio_data, sample_rate,
                                  number_of_channels, number_of_frames,
                                  audio_delay_milliseconds, current_volume,
                                  need_audio_processing);
  }

  return 0;
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
