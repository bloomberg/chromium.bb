// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_receiver_audio_valve.h"

CastReceiverAudioValve::CastReceiverAudioValve(
    media::AudioCapturerSource::CaptureCallback* cb)
    : cb_(cb) {
}
CastReceiverAudioValve::~CastReceiverAudioValve() {}

void CastReceiverAudioValve::Capture(const media::AudioBus* audio_source,
                                     int audio_delay_milliseconds,
                                     double volume,
                                     bool key_pressed) {
  base::AutoLock lock(lock_);
  if (cb_) {
    cb_->Capture(audio_source, audio_delay_milliseconds, volume, key_pressed);
  }
}

void CastReceiverAudioValve::OnCaptureError(const std::string& message) {
  base::AutoLock lock(lock_);
  if (cb_) {
    cb_->OnCaptureError(message);
  }
}

void CastReceiverAudioValve::Stop() {
  base::AutoLock lock(lock_);
  cb_ = nullptr;
}
