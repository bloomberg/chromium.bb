// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_SOURCE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_SOURCE_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/audio_converter.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace media {
class AudioBus;
class AudioConverter;
class AudioFifo;
class AudioParameters;
}

namespace blink {
class WebAudioSourceProviderClient;
}

namespace content {

// WebRtcLocalAudioSourceProvider provides a bridge between classes:
//     WebRtcAudioCapturer ---> blink::WebAudioSourceProvider
//
// WebRtcLocalAudioSourceProvider works as a sink to the WebRtcAudiocapturer
// and store the capture data to a FIFO. When the media stream is connected to
// WebAudio as a source provider, WebAudio will periodically call
// provideInput() to get the data from the FIFO.
//
// All calls are protected by a lock.
class CONTENT_EXPORT WebRtcLocalAudioSourceProvider
    : NON_EXPORTED_BASE(public media::AudioConverter::InputCallback),
      NON_EXPORTED_BASE(public blink::WebAudioSourceProvider) {
 public:
  static const size_t kWebAudioRenderBufferSize;

  WebRtcLocalAudioSourceProvider();
  virtual ~WebRtcLocalAudioSourceProvider();

  // Initialize function for the souce provider. This can be called multiple
  // times if the source format has changed.
  void Initialize(const media::AudioParameters& source_params);

  // Called by the WebRtcAudioCapturer to deliever captured data into fifo on
  // the capture audio thread.
  void DeliverData(media::AudioBus* audio_source,
                   int audio_delay_milliseconds,
                   int volume,
                   bool key_pressed);

  // Called by the WebAudioCapturerSource to get the audio processing params.
  // This function is triggered by provideInput() on the WebAudio audio thread,
  // so it has been under the protection of |lock_|.
  void GetAudioProcessingParams(int* delay_ms, int* volume, bool* key_pressed);

  // blink::WebAudioSourceProvider implementation.
  virtual void setClient(blink::WebAudioSourceProviderClient* client) OVERRIDE;
  virtual void provideInput(const blink::WebVector<float*>& audio_data,
                            size_t number_of_frames) OVERRIDE;

  // media::AudioConverter::Inputcallback implementation.
  // This function is triggered by provideInput()on the WebAudio audio thread,
  // so it has been under the protection of |lock_|.
  virtual double ProvideInput(media::AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE;

  // Method to allow the unittests to inject its own sink parameters to avoid
  // query the hardware.
  // TODO(xians,tommi): Remove and instead offer a way to inject the sink
  // parameters so that the implementation doesn't rely on the global default
  // hardware config but instead gets the parameters directly from the sink
  // (WebAudio in this case). Ideally the unit test should be able to use that
  // same mechanism to inject the sink parameters for testing.
  void SetSinkParamsForTesting(const media::AudioParameters& sink_params);

 private:
  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_ptr<media::AudioConverter> audio_converter_;
  scoped_ptr<media::AudioFifo> fifo_;
  scoped_ptr<media::AudioBus> bus_wrapper_;
  int audio_delay_ms_;
  int volume_;
  bool key_pressed_;
  bool is_enabled_;
  media::AudioParameters source_params_;
  media::AudioParameters sink_params_;

  // Protects all the member variables above.
  base::Lock lock_;

  // Used to report the correct delay to |webaudio_source_|.
  base::TimeTicks last_fill_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioSourceProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_SOURCE_PROVIDER_H_
