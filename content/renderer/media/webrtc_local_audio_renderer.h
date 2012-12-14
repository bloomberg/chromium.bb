// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace media {
class AudioBus;
class AudioParameters;
}

namespace content {

class RendererAudioOutputDevice;
class WebRtcAudioCapturer;

// WebRtcLocalAudioRenderer is a webkit_media::MediaStreamAudioRenderer
// designed for rendering local audio media stream tracks,
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
// It also implements media::AudioRendererSink::RenderCallback to render audio
// data provided from a WebRtcAudioCapturer source which is set at construction.
// When the audio layer in the browser process asks for data to render, this
// class provides the data by reading from the source using the registered
// WebRtcAudioCapturer source.
// TODO(henrika): improve by using similar principles as in RTCVideoRenderer
// which register itself to the video track when the provider is started and
// deregisters itself when it is stopped.
// Tracking this at http://crbug.com/164813.
class CONTENT_EXPORT WebRtcLocalAudioRenderer
    : NON_EXPORTED_BASE(public webkit_media::MediaStreamAudioRenderer) {
 public:

  class LocalRenderCallback {
   public:
    // Audio data is provided to the caller using this callback.
    virtual void ProvideInput(media::AudioBus* dest) = 0;

   protected:
    virtual ~LocalRenderCallback() {}
  };

  // Creates a local renderer and registers a capturing |source| object.
  // The |source| is owned by the WebRtcAudioDeviceImpl.
  // Called on the main thread.
  WebRtcLocalAudioRenderer(const scoped_refptr<WebRtcAudioCapturer>& source,
                           int source_render_view_id);

  // webkit_media::MediaStreamAudioRenderer implementation.
  // Called on the main thread.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;
  virtual base::TimeDelta GetCurrentRenderTime() const OVERRIDE;
  virtual bool IsLocalRenderer() const OVERRIDE;

  bool Started() const { return (callback_ != NULL); }

 protected:
  virtual ~WebRtcLocalAudioRenderer();

 private:
  // Called by the WebRtcAudioCapturer when the capture device has stopped.
  void OnSourceCaptureDeviceStopped();

  // Private class which implements AudioRendererSink::RenderCallback
  // and also wraps members which can be accesses both on the main render
  // thread and the AudioOutputDevice media thread.
  class AudioCallback;

  // The actual WebRtcLocalAudioRenderer::AudioCallback instance is created
  // in Start() and released in Stop().
  scoped_ptr<WebRtcLocalAudioRenderer::AudioCallback> callback_;

  // The source of data to render. Given that this class implements local
  // loopback, the source is a capture instance reading data from the
  // selected microphone. The recorded data is stored in a FIFO and consumed
  // by this class when the sink asks for new data.
  // The WebRtcAudioCapturer is today created by WebRtcAudioDeviceImpl.
  scoped_refptr<WebRtcAudioCapturer> source_;

  // The render view in which the audio is rendered into |sink_|.
  const int source_render_view_id_;

  // The sink (destination) for rendered audio.
  scoped_refptr<RendererAudioOutputDevice> sink_;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
