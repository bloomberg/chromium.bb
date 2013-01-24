// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer_sink.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace content {

class RendererAudioOutputDevice;
class WebRtcAudioRendererSource;

// This renderer handles calls from the pipeline and WebRtc ADM. It is used
// for connecting WebRtc MediaStream with pipeline.
class CONTENT_EXPORT WebRtcAudioRenderer
    : NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public webkit_media::MediaStreamAudioRenderer) {
 public:
  explicit WebRtcAudioRenderer(int source_render_view_id);

  // Initialize function called by clients like WebRtcAudioDeviceImpl. Note,
  // Stop() has to be called before |source| is deleted.
  // Returns false if Initialize() fails.
  bool Initialize(WebRtcAudioRendererSource* source);

  // Methods called by WebMediaPlayerMS and WebRtcAudioDeviceImpl.
  // MediaStreamAudioRenderer implementation.
  virtual void Start() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;
  virtual base::TimeDelta GetCurrentRenderTime() const OVERRIDE;
  virtual bool IsLocalRenderer() const OVERRIDE;

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

  // The render view in which the audio is rendered into |sink_|.
  const int source_render_view_id_;

  // The sink (destination) for rendered audio.
  scoped_refptr<RendererAudioOutputDevice> sink_;

  // Audio data source from the browser process.
  WebRtcAudioRendererSource* source_;

  // Cached values of utilized audio parameters. Platform dependent.
  media::AudioParameters params_;

  // Buffers used for temporary storage during render callbacks.
  // Allocated during initialization.
  scoped_array<int16> buffer_;

  // Protect access to |state_|.
  base::Lock lock_;

  // Ref count for the MediaPlayers which are playing audio.
  int play_ref_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
