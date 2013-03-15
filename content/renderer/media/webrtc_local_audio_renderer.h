// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "webkit/media/media_stream_audio_renderer.h"

namespace media {
class AudioBus;
class AudioParameters;
}

namespace webrtc {
class AudioTrackInterface;
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
// class provides the data by implementing the WebRtcAudioCapturerSink
// interface, i.e., we are a sink seen from the WebRtcAudioCapturer perspective.
// TODO(henrika): improve by using similar principles as in RTCVideoRenderer
// which register itself to the video track when the provider is started and
// deregisters itself when it is stopped.
// Tracking this at http://crbug.com/164813.
class CONTENT_EXPORT WebRtcLocalAudioRenderer
    : NON_EXPORTED_BASE(public webkit_media::MediaStreamAudioRenderer),
      NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public WebRtcAudioCapturerSink),
      NON_EXPORTED_BASE(public webrtc::ObserverInterface) {
 public:
  // Creates a local renderer and registers a capturing |source| object.
  // The |source| is owned by the WebRtcAudioDeviceImpl.
  // Called on the main thread.
  WebRtcLocalAudioRenderer(const scoped_refptr<WebRtcAudioCapturer>& source,
                           webrtc::AudioTrackInterface* audio_track,
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

  // content::WebRtcAudioCapturerSink implementation.

  // Called on the AudioInputDevice worker thread.
  virtual void CaptureData(const int16* audio_data,
                           int number_of_channels,
                           int number_of_frames,
                           int audio_delay_milliseconds,
                           double volume) OVERRIDE;

  // Can be called on different user thread.
  virtual void SetCaptureFormat(const media::AudioParameters& params) OVERRIDE;

  // media::AudioRendererSink::RenderCallback implementation.
  // Render() is called on the AudioOutputDevice thread and OnRenderError()
  // on the IO thread.
  virtual int Render(media::AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // webrtc::ObserverInterface implementation.
  // Called on the main render thread.
  virtual void OnChanged() OVERRIDE;

  base::TimeDelta total_render_time() const { return total_render_time_; }

 protected:
  virtual ~WebRtcLocalAudioRenderer();

 private:
  // The source of data to render. Given that this class implements local
  // loopback, the source is a capture instance reading data from the
  // selected microphone. The recorded data is stored in a FIFO and consumed
  // by this class when the sink asks for new data.
  // The WebRtcAudioCapturer is today created by WebRtcAudioDeviceImpl.
  scoped_refptr<WebRtcAudioCapturer> source_;

  scoped_refptr<webrtc::AudioTrackInterface> audio_track_;

  // The render view in which the audio is rendered into |sink_|.
  const int source_render_view_id_;

  // The sink (destination) for rendered audio.
  scoped_refptr<RendererAudioOutputDevice> sink_;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Contains copies of captured audio frames.
  scoped_ptr<media::AudioFifo> loopback_fifo_;

  // Stores last time a render callback was received. The time difference
  // between a new time stamp and this value can be used to derive the
  // total render time.
  base::Time last_render_time_;

  // Keeps track of total time audio has been rendered.
  base::TimeDelta total_render_time_;

  // Set when playing, cleared when paused.
  bool playing_;

  // Stores latest media track state for the enabled attribute.
  bool track_is_enabled_;

  // Protects |loopback_fifo_|, |playing_| and |sink_|.
  mutable base::Lock thread_lock_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
