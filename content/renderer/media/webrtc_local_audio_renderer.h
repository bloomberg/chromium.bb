// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_audio_renderer.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/base/output_device.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class AudioBus;
class AudioShifter;
class AudioOutputDevice;
class AudioParameters;
}

namespace content {

class WebRtcAudioCapturer;

// WebRtcLocalAudioRenderer is a MediaStreamAudioRenderer designed for rendering
// local audio media stream tracks,
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
// It also implements media::AudioRendererSink::RenderCallback to render audio
// data provided from a WebRtcLocalAudioTrack source.
// When the audio layer in the browser process asks for data to render, this
// class provides the data by implementing the MediaStreamAudioSink
// interface, i.e., we are a sink seen from the WebRtcAudioCapturer perspective.
// TODO(henrika): improve by using similar principles as in
// MediaStreamVideoRendererSink which register itself to the video track when
// the provider is started and deregisters itself when it is stopped. Tracking
// this at http://crbug.com/164813.
class CONTENT_EXPORT WebRtcLocalAudioRenderer
    : NON_EXPORTED_BASE(public MediaStreamAudioRenderer),
      NON_EXPORTED_BASE(public MediaStreamAudioSink),
      NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback),
      NON_EXPORTED_BASE(public media::OutputDevice) {
 public:
  // Creates a local renderer and registers a capturing |source| object.
  // The |source| is owned by the WebRtcAudioDeviceImpl.
  // Called on the main thread.
  WebRtcLocalAudioRenderer(const blink::WebMediaStreamTrack& audio_track,
                           int source_render_frame_id,
                           int session_id,
                           const std::string& device_id,
                           const url::Origin& security_origin);

  // MediaStreamAudioRenderer implementation.
  // Called on the main thread.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  void SetVolume(float volume) override;
  media::OutputDevice* GetOutputDevice() override;
  base::TimeDelta GetCurrentRenderTime() const override;
  bool IsLocalRenderer() const override;

  // media::OutputDevice implementation
  void SwitchOutputDevice(const std::string& device_id,
                          const url::Origin& security_origin,
                          const media::SwitchOutputDeviceCB& callback) override;
  media::AudioParameters GetOutputParameters() override;
  media::OutputDeviceStatus GetDeviceStatus() override;

  const base::TimeDelta& total_render_time() const {
    return total_render_time_;
  }

 protected:
  ~WebRtcLocalAudioRenderer() override;

 private:
  // MediaStreamAudioSink implementation.

  // Called on the AudioInputDevice worker thread.
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;

  // Called on the AudioInputDevice worker thread.
  void OnSetFormat(const media::AudioParameters& params) override;

  // media::AudioRendererSink::RenderCallback implementation.
  // Render() is called on the AudioOutputDevice thread and OnRenderError()
  // on the IO thread.
  int Render(media::AudioBus* audio_bus,
             uint32_t audio_delay_milliseconds,
             uint32_t frames_skipped) override;
  void OnRenderError() override;

  // Initializes and starts the |sink_| if
  //  we have received valid |source_params_| &&
  //  |playing_| has been set to true &&
  //  |volume_| is not zero.
  void MaybeStartSink();

  // Sets new |source_params_| and then re-initializes and restarts |sink_|.
  void ReconfigureSink(const media::AudioParameters& params);

  // The audio track which provides data to render. Given that this class
  // implements local loopback, the audio track is getting data from a capture
  // instance like a selected microphone and forwards the recorded data to its
  // sinks. The recorded data is stored in a FIFO and consumed
  // by this class when the sink asks for new data.
  // This class is calling MediaStreamAudioSink::AddToAudioTrack() and
  // MediaStreamAudioSink::RemoveFromAudioTrack() to connect and disconnect
  // with the audio track.
  blink::WebMediaStreamTrack audio_track_;

  // The render view and frame in which the audio is rendered into |sink_|.
  const int source_render_frame_id_;
  const int session_id_;

  // MessageLoop associated with the single thread that performs all control
  // tasks.  Set to the MessageLoop that invoked the ctor.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioOutputDevice> sink_;

  // This does all the synchronization/resampling/smoothing.
  scoped_ptr<media::AudioShifter> audio_shifter_;

  // Stores last time a render callback was received. The time difference
  // between a new time stamp and this value can be used to derive the
  // total render time.
  base::TimeTicks last_render_time_;

  // Keeps track of total time audio has been rendered.
  base::TimeDelta total_render_time_;

  // The audio parameters of the capture source.
  // Must only be touched on the main thread.
  media::AudioParameters source_params_;

  // The audio parameters used by the sink.
  // Must only be touched on the main thread.
  media::AudioParameters sink_params_;

  // Set when playing, cleared when paused.
  bool playing_;

  // Protects |audio_shifter_|, |playing_|, |last_render_time_|,
  // |total_render_time_| and |volume_|.
  mutable base::Lock thread_lock_;

  // The preferred device id of the output device or empty for the default
  // output device.
  std::string output_device_id_;
  url::Origin security_origin_;

  // Cache value for the volume.
  float volume_;

  // Flag to indicate whether |sink_| has been started yet.
  bool sink_started_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOCAL_AUDIO_RENDERER_H_
