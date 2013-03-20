// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_fifo.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

namespace content {

class WebRtcAudioCapturer;

// WebAudioCapturerSource is the missing link between
// WebAudio's MediaStreamAudioDestinationNode and WebRtcAudioCapturer.
//
// 1. WebKit calls the setFormat() method setting up the basic stream format
//    (channels, and sample-rate).  At this time, it dispatches this information
//    to the WebRtcAudioCapturer by calling its SetCapturerSource() method.
// 2. Initialize() is called, where we should get back the same
//    stream format information as (1).  We also get the CaptureCallback here.
// 3. consumeAudio() is called periodically by WebKit which dispatches the
//    audio stream to the CaptureCallback::Capture() method.
class WebAudioCapturerSource
    : public media::AudioCapturerSource,
      public WebKit::WebAudioDestinationConsumer {
 public:
  explicit WebAudioCapturerSource(WebRtcAudioCapturer* capturer);

  // WebAudioDestinationConsumer implementation.
  // setFormat() is called early on, so that we can configure the capturer.
  virtual void setFormat(size_t number_of_channels, float sample_rate) OVERRIDE;
  // MediaStreamAudioDestinationNode periodically calls consumeAudio().
  virtual void consumeAudio(const WebKit::WebVector<const float*>& audio_data,
      size_t number_of_frames) OVERRIDE;

  // AudioCapturerSource implementation.
  virtual void Initialize(
      const media::AudioParameters& params,
      media::AudioCapturerSource::CaptureCallback* callback,
      int session_id) OVERRIDE;

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE { }
  virtual void SetAutomaticGainControl(bool enable) OVERRIDE { }

 private:
  virtual ~WebAudioCapturerSource();

  WebRtcAudioCapturer* capturer_;

  int set_format_channels_;
  media::AudioParameters params_;
  media::AudioCapturerSource::CaptureCallback* callback_;

  // Wraps data coming from HandleCapture().
  scoped_ptr<media::AudioBus> wrapper_bus_;

  // Bus for reading from FIFO and calling the CaptureCallback.
  scoped_ptr<media::AudioBus> capture_bus_;

  // Handles mismatch between WebAudio buffer size and WebRTC.
  scoped_ptr<media::AudioFifo> fifo_;

  // Synchronizes HandleCapture() with AudioCapturerSource calls.
  base::Lock lock_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(WebAudioCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
