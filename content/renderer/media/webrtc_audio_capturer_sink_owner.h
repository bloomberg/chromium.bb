// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_SINK_OWNER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_SINK_OWNER_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_parameters.h"

namespace content {

class WebRtcAudioCapturerSink;

// Reference counted container of WebRtcAudioCapturerSink delegates.
class WebRtcAudioCapturerSinkOwner
    : public base::RefCountedThreadSafe<WebRtcAudioCapturerSinkOwner>,
      public WebRtcAudioCapturerSink {
 public:
  explicit WebRtcAudioCapturerSinkOwner(WebRtcAudioCapturerSink* sink);

  // WebRtcAudioCapturerSink implementation.
  virtual void CaptureData(const int16* audio_data,
                           int number_of_channels,
                           int number_of_frames,
                           int audio_delay_milliseconds,
                           double volume) OVERRIDE;
  virtual void SetCaptureFormat(const media::AudioParameters& params) OVERRIDE;

  bool IsEqual(const WebRtcAudioCapturerSink* other) const;
  void Reset();

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct WrapsSink {
    WrapsSink(WebRtcAudioCapturerSink* sink) : sink_(sink) {}
    bool operator()(
        const scoped_refptr<WebRtcAudioCapturerSinkOwner>& owner) {
      return owner->IsEqual(sink_);
    }
    WebRtcAudioCapturerSink* sink_;
  };

 protected:
  virtual ~WebRtcAudioCapturerSinkOwner() {}

 private:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturerSinkOwner>;
  WebRtcAudioCapturerSink* delegate_;
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioCapturerSinkOwner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_SINK_OWNER_H_
