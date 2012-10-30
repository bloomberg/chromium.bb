// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer_sink.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace content {

class WebRtcAudioRendererSource;

// This renderer handles calls from the pipeline and WebRtc ADM. It is used
// for connecting WebRtc MediaStream with pipeline.
class CONTENT_EXPORT WebRtcAudioRenderer
    : NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public webkit_media::MediaStreamAudioRenderer) {
 public:
  WebRtcAudioRenderer();

  // Initialize function called by clients like WebRtcAudioDeviceImpl. Note,
  // Stop() has to be called before |source| is deleted.
  // Returns false if Initialize() fails.
  bool Initialize(WebRtcAudioRendererSource* source);

  // Methods called by WebMediaPlayerMS and WebRtcAudioDeviceImpl.
  // MediaStreamAudioRenderer implementation.
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;

 protected:
  virtual ~WebRtcAudioRenderer();

 private:
  enum State {
    UNINITIALIZED,
    PLAYING,
    PAUSED,
  };
  // Flag to keep track the state of the renderer.
  State state_;

  // media::AudioRendererSink::RenderCallback implementation.
  virtual int Render(media::AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioRendererSink> sink_;

  // Audio data source from the browser process.
  WebRtcAudioRendererSource* source_;

  // Cached values of utilized audio parameters. Platform dependent.
  media::AudioParameters params_;

  // Buffers used for temporary storage during render callbacks.
  // Allocated during initialization.
  scoped_array<int16> buffer_;

  // Protect access to |state_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_CAPTURER_H_
