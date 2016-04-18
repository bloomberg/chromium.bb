// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_push_fifo.h"
#include "third_party/WebKit/public/platform/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class WebRtcLocalAudioTrack;

// WebAudioCapturerSource is the missing link between
// WebAudio's MediaStreamAudioDestinationNode and WebRtcLocalAudioTrack.
//
// 1. WebKit calls the setFormat() method setting up the basic stream format
//    (channels, and sample-rate).
// 2. consumeAudio() is called periodically by WebKit which dispatches the
//    audio stream to the WebRtcLocalAudioTrack::Capture() method.
class WebAudioCapturerSource : public blink::WebAudioDestinationConsumer {
 public:
  explicit WebAudioCapturerSource(blink::WebMediaStreamSource* blink_source);

  ~WebAudioCapturerSource() override;

  // WebAudioDestinationConsumer implementation.
  // setFormat() is called early on, so that we can configure the audio track.
  void setFormat(size_t number_of_channels, float sample_rate) override;
  // MediaStreamAudioDestinationNode periodically calls consumeAudio().
  // Called on the WebAudio audio thread.
  void consumeAudio(const blink::WebVector<const float*>& audio_data,
                    size_t number_of_frames) override;

  // Called when the WebAudioCapturerSource is hooking to a media audio track.
  // |track| is the sink of the data flow and must remain alive until Stop() is
  // called.
  void Start(WebRtcLocalAudioTrack* track);

  // Called when the media audio track is stopping.
  void Stop();

 private:
  // Called by AudioPushFifo zero or more times during the call to
  // consumeAudio().  Delivers audio data with the required buffer size to the
  // track.
  void DeliverRebufferedAudio(const media::AudioBus& audio_bus,
                              int frame_delay);

  // Deregisters this object from its blink::WebMediaStreamSource.
  void DeregisterFromBlinkSource();

  // Used to DCHECK that some methods are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // The audio track this WebAudioCapturerSource is feeding data to.
  WebRtcLocalAudioTrack* track_;

  media::AudioParameters params_;

  // Flag to help notify the |track_| when the audio format has changed.
  bool audio_format_changed_;

  // A wrapper used for providing audio to |fifo_|.
  std::unique_ptr<media::AudioBus> wrapper_bus_;

  // Takes in the audio data passed to consumeAudio() and re-buffers it into 10
  // ms chunks for the track.  This ensures each chunk of audio delivered to the
  // track has the required buffer size, regardless of the amount of audio
  // provided via each consumeAudio() call.
  media::AudioPushFifo fifo_;

  // Used to pass the reference timestamp between DeliverDecodedAudio() and
  // DeliverRebufferedAudio().
  base::TimeTicks current_reference_time_;

  // Synchronizes HandleCapture() with AudioCapturerSource calls.
  base::Lock lock_;

  // This object registers with a blink::WebMediaStreamSource. We keep track of
  // that in order to be able to deregister before stopping the audio track.
  blink::WebMediaStreamSource blink_source_;

  DISALLOW_COPY_AND_ASSIGN(WebAudioCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBAUDIO_CAPTURER_SOURCE_H_
