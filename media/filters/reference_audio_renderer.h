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
// See src/chrome/renderer/media/audio_renderer_impl.h for chrome's
// implementation.

#include <deque>

#include "media/audio/audio_io.h"
#include "media/base/buffers.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

class MEDIA_EXPORT ReferenceAudioRenderer
    : public AudioRendererBase,
      public AudioOutputStream::AudioSourceCallback {
 public:
  ReferenceAudioRenderer();
  virtual ~ReferenceAudioRenderer();

  // Filter implementation.
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;

  // AudioRenderer implementation.
  virtual void SetVolume(float volume) OVERRIDE;

  // AudioSourceCallback implementation.
  virtual uint32 OnMoreData(AudioOutputStream* stream, uint8* dest,
                            uint32 len,
                            AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code) OVERRIDE;

 protected:
  // AudioRendererBase implementation.
  virtual bool OnInitialize(int bits_per_channel,
                            ChannelLayout channel_layout,
                            int sample_rate) OVERRIDE;
  virtual void OnStop() OVERRIDE;

 private:
  // Audio output stream device.
  AudioOutputStream* stream_;
  int bytes_per_second_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceAudioRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_REFERENCE_AUDIO_RENDERER_H_
