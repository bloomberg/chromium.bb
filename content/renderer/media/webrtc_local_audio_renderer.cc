// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_renderer.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/media_switches.h"

namespace content {

// WebRtcLocalAudioRenderer::AudioCallback wraps the AudioOutputDevice thread,
// receives callbacks on that thread and proxies these requests to the capture
// sink.
class WebRtcLocalAudioRenderer::AudioCallback
    : public media::AudioRendererSink::RenderCallback {
 public:
  explicit AudioCallback(LocalRenderCallback* source);
  virtual ~AudioCallback();

  // media::AudioRendererSink::RenderCallback implementation.
  // Render() is called on the AudioOutputDevice thread and OnRenderError()
  // on the IO thread.
  virtual int Render(media::AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  void Start();
  void Play();
  void Pause();
  bool Playing() const;

  base::TimeDelta total_render_time() const { return total_render_time_; }

 private:
  // Get data from this source on each render request.
  LocalRenderCallback* source_;

  // Set when playing, cleared when paused.
  base::subtle::Atomic32 playing_;

  // Stores last time a render callback was received. The time difference
  // between a new time stamp and this value can be used to derive the
  // total render time.
  base::Time last_render_time_;

  // Keeps track of total time audio has been rendered.
  base::TimeDelta total_render_time_;

  // Protects |total_render_time_| and |source_capture_device_has_stopped_
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioCallback);
};

// WebRtcLocalAudioRenderer::AudioCallback implementation.
WebRtcLocalAudioRenderer::AudioCallback::AudioCallback(
    LocalRenderCallback* source)
    : source_(source),
      playing_(false) {
}

WebRtcLocalAudioRenderer::AudioCallback::~AudioCallback() {}

void WebRtcLocalAudioRenderer::AudioCallback::Start() {
  last_render_time_ = base::Time::Now();
  base::subtle::Release_Store(&playing_, 0);
}

void WebRtcLocalAudioRenderer::AudioCallback::Play() {
  base::subtle::Release_Store(&playing_, 1);
  last_render_time_ = base::Time::Now();
}

void WebRtcLocalAudioRenderer::AudioCallback::Pause() {
  base::subtle::Release_Store(&playing_, 0);
}

bool WebRtcLocalAudioRenderer::AudioCallback::Playing() const {
  return (base::subtle::Acquire_Load(&playing_) != false);
}

// media::AudioRendererSink::RenderCallback implementation
int WebRtcLocalAudioRenderer::AudioCallback::Render(
    media::AudioBus* audio_bus, int audio_delay_milliseconds) {

  if (!Playing())
    return 0;

  base::Time now = base::Time::Now();
  {
    base::AutoLock auto_lock(lock_);
    total_render_time_ += now - last_render_time_;
  }
  last_render_time_ = now;

  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::AudioCallback::Render");

  // Acquire audio samples from the FIFO owned by the capturing source.
  source_->ProvideInput(audio_bus);
  return audio_bus->frames();
}

void WebRtcLocalAudioRenderer::AudioCallback::OnRenderError() {
  NOTIMPLEMENTED();
}

// WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer implementation.
WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer(
    const scoped_refptr<WebRtcAudioCapturer>& source,
    int source_render_view_id)
    : source_(source),
      source_render_view_id_(source_render_view_id) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer()";
}

WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!Started());
  DVLOG(1) << "WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer()";
}

void WebRtcLocalAudioRenderer::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!Started());
  DVLOG(1) << "WebRtcLocalAudioRenderer::Start()";

  if (source_->IsInLoopbackMode()) {
    DLOG(ERROR) << "Only one local renderer is supported";
    return;
  }

  // Create the audio callback instance and attach the capture source to ensure
  // that the renderer can read data from the loopback buffer in the capture
  // source.
  callback_.reset(new WebRtcLocalAudioRenderer::AudioCallback(source_));

  // Register the local renderer with its source. We are a sink seen from the
  // source perspective.
  const base::Closure& on_device_stopped_cb = media::BindToCurrentLoop(
      base::Bind(
          &WebRtcLocalAudioRenderer::OnSourceCaptureDeviceStopped, this));
  source_->PrepareLoopback();
  source_->SetStopCallback(on_device_stopped_cb);

  // Use the capturing source audio parameters when opening the output audio
  // device. Any mismatch will be compensated for by the audio output back-end.
  // Note that the buffer size is modified to make the full-duplex scheme less
  // resource intensive. By doubling the buffer size (compared to the capture
  // side), the callback frequency of browser side callbacks will be lower and
  // tests have shown that it resolves issues with audio glitches for some
  // cases where resampling is needed on the output side.
  // TODO(henrika): verify this scheme on as many different devices and
  // combinations of sample rates as possible
  media::AudioParameters source_params = source_->audio_parameters();
  media::AudioParameters sink_params(source_params.format(),
                                     source_params.channel_layout(),
                                     source_params.sample_rate(),
                                     source_params.bits_per_sample(),
                                     2 * source_params.frames_per_buffer());
  sink_ = AudioDeviceFactory::NewOutputDevice();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAudioInput)) {
    // TODO(henrika): we could utilize the unified audio here instead and do
    // sink_->InitializeIO(sink_params, 2, callback_.get());
    // It would then be possible to avoid using the WebRtcAudioCapturer.
    DVLOG(1) << "enable-webaudio-input command-line flag is enabled";
  }
  sink_->Initialize(sink_params, callback_.get());
  sink_->SetSourceRenderView(source_render_view_id_);

  // Start local rendering and the capturer. Note that, the capturer is owned
  // by the WebRTC ADM and might already bee running.
  source_->Start();
  sink_->Start();
  callback_->Start();
}

void WebRtcLocalAudioRenderer::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::Stop()";

  if (!Started())
    return;

  // Stop the output audio stream, i.e, stop asking for data to render.
  sink_->Stop();
  sink_ = NULL;

  // Delete the audio callback before deleting the source since the callback
  // is using the source.
  callback_.reset();

  // Unregister this class as sink to the capturing source and invalidate the
  // source pointer. This call clears the local FIFO in the capturer, and at
  // same time, ensure that recorded data is no longer added to the FIFO.
  // Note that, we do not stop the capturer here since it may still be used by
  // the WebRTC ADM.
  source_->CancelLoopback();
  source_ = NULL;
}

void WebRtcLocalAudioRenderer::Play() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::Play()";

  if (!Started())
    return;

  // Resumes rendering by ensuring that WebRtcLocalAudioRenderer::Render()
  // now reads data from the local FIFO in the capturing source.
  callback_->Play();
  source_->ResumeBuffering();
}

void WebRtcLocalAudioRenderer::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::Pause()";

  if (!Started())
    return;

  // Temporarily suspends rendering audio.
  // WebRtcLocalAudioRenderer::Render() will return early during this state
  // and no data will be provided to the active sink.
  callback_->Pause();
  source_->PauseBuffering();
}

void WebRtcLocalAudioRenderer::SetVolume(float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::SetVolume(" << volume << ")";
  if (sink_)
    sink_->SetVolume(volume);
}

base::TimeDelta WebRtcLocalAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!Started())
    return base::TimeDelta();
  return callback_->total_render_time();
}

bool WebRtcLocalAudioRenderer::IsLocalRenderer() const {
  return true;
}

void WebRtcLocalAudioRenderer::OnSourceCaptureDeviceStopped() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::OnSourceCaptureDeviceStopped()";
  if (!Started())
    return;

  // The capture device has stopped and we should therefore stop all activity
  // as well to save resources.
  Stop();
}

}  // namespace content
