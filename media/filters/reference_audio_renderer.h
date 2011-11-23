// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_REFERENCE_AUDIO_RENDERER_H_
#define MEDIA_FILTERS_REFERENCE_AUDIO_RENDERER_H_

// This is the reference implementation of AudioRenderer, which uses the audio
// interfaces to open an audio device.  It cannot be used in the sandbox, but is
// used in other applications such as the test player.
//
// Note: THIS IS NOT THE AUDIO RENDERER USED IN CHROME.
//
// See src/content/renderer/media/audio_renderer_impl.h for chrome's
// implementation.

#include "media/audio/audio_output_controller.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

class MEDIA_EXPORT ReferenceAudioRenderer
    : public AudioRendererBase,
      public AudioOutputController::EventHandler {
 public:
  ReferenceAudioRenderer();
  virtual ~ReferenceAudioRenderer();

  // Filter implementation.
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;

  // AudioRenderer implementation.
  virtual void SetVolume(float volume) OVERRIDE;

  // AudioController::EventHandler implementation.
  virtual void OnCreated(AudioOutputController* controller) OVERRIDE;
  virtual void OnPlaying(AudioOutputController* controller) OVERRIDE;
  virtual void OnPaused(AudioOutputController* controller) OVERRIDE;
  virtual void OnError(AudioOutputController* controller,
                       int error_code) OVERRIDE;
  virtual void OnMoreData(AudioOutputController* controller,
                          AudioBuffersState buffers_state) OVERRIDE;

 protected:
  // AudioRendererBase implementation.
  virtual bool OnInitialize(int bits_per_channel,
                            ChannelLayout channel_layout,
                            int sample_rate) OVERRIDE;
  virtual void OnStop() OVERRIDE;

 private:
  int bytes_per_second_;

  // AudioOutputController::Close callback.
  virtual void OnClose();

  // Audio output controller.
  scoped_refptr<media::AudioOutputController> controller_;

  // Audio buffer.
  int buffer_capacity_;
  scoped_array<uint8> buffer_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceAudioRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_REFERENCE_AUDIO_RENDERER_H_
