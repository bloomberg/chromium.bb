// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_renderer_sink.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace media {
class AudioOutputDevice;
}

namespace content {

class WebRtcAudioRendererSource;

// This renderer handles calls from the pipeline and WebRtc ADM. It is used
// for connecting WebRtc MediaStream with the audio pipeline.
class CONTENT_EXPORT WebRtcAudioRenderer
    : NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public webkit_media::MediaStreamAudioRenderer) {
 public:
  explicit WebRtcAudioRenderer(int source_render_view_id);

  // Initialize function called by clients like WebRtcAudioDeviceImpl.
  // Stop() has to be called before |source| is deleted.
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

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Flag to keep track the state of the renderer.
  State state_;

  // media::AudioRendererSink::RenderCallback implementation.
  // These two methods are called on the AudioOutputDevice worker thread.
  virtual int Render(media::AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Called by AudioPullFifo when more data is necessary.
  // This method is called on the AudioOutputDevice worker thread.
  void SourceCallback(int fifo_frame_delay, media::AudioBus* audio_bus);

  // The render view in which the audio is rendered into |sink_|.
  const int source_render_view_id_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioOutputDevice> sink_;

  // Audio data source from the browser process.
  WebRtcAudioRendererSource* source_;

  // Buffers used for temporary storage during render callbacks.
  // Allocated during initialization.
  scoped_ptr<int16[]> buffer_;

  // Protects access to |state_|, |source_| and |sink_|.
  base::Lock lock_;

  // Ref count for the MediaPlayers which are playing audio.
  int play_ref_count_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  scoped_ptr<media::AudioPullFifo> audio_fifo_;

  // Contains the accumulated delay estimate which is provided to the WebRTC
  // AEC.
  int audio_delay_milliseconds_;

  // Lengh of an audio frame in milliseconds.
  double frame_duration_milliseconds_;

  double fifo_io_ratio_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_RENDERER_H_
