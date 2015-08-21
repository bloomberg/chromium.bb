// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_RECEIVER_AUDIO_VALVE_H_
#define CHROME_RENDERER_MEDIA_CAST_RECEIVER_AUDIO_VALVE_H_

#include "base/synchronization/lock.h"
#include "media/base/audio_capturer_source.h"

namespace media {
class AudioBus;
}

// Forwards calls to |cb| until Stop is called.
// Thread-safe.
// All functions may block depending on contention.
class CastReceiverAudioValve :
    public media::AudioCapturerSource::CaptureCallback,
    public base::RefCountedThreadSafe<CastReceiverAudioValve> {
 public:
  explicit CastReceiverAudioValve(
      media::AudioCapturerSource::CaptureCallback* cb);

  // AudioCapturerSource::CaptureCallback implementation.
  void Capture(const media::AudioBus* audio_source,
               int audio_delay_milliseconds,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(const std::string& message) override;

  // When this returns, no more calls will be forwarded to |cb|.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<CastReceiverAudioValve>;
  ~CastReceiverAudioValve() override;
  media::AudioCapturerSource::CaptureCallback* cb_;
  base::Lock lock_;
};

#endif  // CHROME_RENDERER_MEDIA_CAST_RECEIVER_AUDIO_VALVE_H_
