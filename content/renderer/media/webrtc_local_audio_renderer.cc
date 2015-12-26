// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_renderer.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_shifter.h"

namespace content {

namespace {

enum LocalRendererSinkStates {
  kSinkStarted = 0,
  kSinkNeverStarted,
  kSinkStatesMax  // Must always be last!
};

}  // namespace

// media::AudioRendererSink::RenderCallback implementation
int WebRtcLocalAudioRenderer::Render(media::AudioBus* audio_bus,
                                     uint32_t audio_delay_milliseconds,
                                     uint32_t frames_skipped) {
  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::Render");
  base::AutoLock auto_lock(thread_lock_);

  if (!playing_ || !volume_ || !audio_shifter_) {
    audio_bus->Zero();
    return 0;
  }

  audio_shifter_->Pull(
      audio_bus,
      base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds));

  return audio_bus->frames();
}

void WebRtcLocalAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
}

// content::MediaStreamAudioSink implementation
void WebRtcLocalAudioRenderer::OnData(const media::AudioBus& audio_bus,
                                      base::TimeTicks estimated_capture_time) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(!estimated_capture_time.is_null());

  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::CaptureData");

  base::AutoLock auto_lock(thread_lock_);
  if (!playing_ || !volume_ || !audio_shifter_)
    return;

  scoped_ptr<media::AudioBus> audio_data(
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames()));
  audio_bus.CopyTo(audio_data.get());
  audio_shifter_->Push(std::move(audio_data), estimated_capture_time);
  const base::TimeTicks now = base::TimeTicks::Now();
  total_render_time_ += now - last_render_time_;
  last_render_time_ = now;
}

void WebRtcLocalAudioRenderer::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::OnSetFormat()";
  // If the source is restarted, we might have changed to another capture
  // thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  // Post a task on the main render thread to reconfigure the |sink_| with the
  // new format.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcLocalAudioRenderer::ReconfigureSink, this, params));
}

// WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer implementation.
WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int source_render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin)
    : audio_track_(audio_track),
      source_render_frame_id_(source_render_frame_id),
      session_id_(session_id),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      playing_(false),
      output_device_id_(device_id),
      security_origin_(security_origin),
      volume_(0.0),
      sink_started_(false) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer()";
}

WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!sink_.get());
  DVLOG(1) << "WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer()";
}

void WebRtcLocalAudioRenderer::Start() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Start()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // We get audio data from |audio_track_|...
  MediaStreamAudioSink::AddToAudioTrack(this, audio_track_);
  // ...and |sink_| will get audio data from us.
  DCHECK(!sink_.get());
  sink_ =
      AudioDeviceFactory::NewOutputDevice(source_render_frame_id_, session_id_,
                                          output_device_id_, security_origin_);

  base::AutoLock auto_lock(thread_lock_);
  last_render_time_ = base::TimeTicks::Now();
  playing_ = false;
}

void WebRtcLocalAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Stop()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    playing_ = false;
    audio_shifter_.reset();
  }

  // Stop the output audio stream, i.e, stop asking for data to render.
  // It is safer to call Stop() on the |sink_| to clean up the resources even
  // when the |sink_| is never started.
  if (sink_.get()) {
    sink_->Stop();
    sink_ = NULL;
  }

  if (!sink_started_) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                              kSinkNeverStarted, kSinkStatesMax);
  }
  sink_started_ = false;

  // Ensure that the capturer stops feeding us with captured audio.
  MediaStreamAudioSink::RemoveFromAudioTrack(this, audio_track_);
}

void WebRtcLocalAudioRenderer::Play() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Play()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  {
    base::AutoLock auto_lock(thread_lock_);
    // Resumes rendering by ensuring that WebRtcLocalAudioRenderer::Render()
    // now reads data from the local FIFO.
    playing_ = true;
    last_render_time_ = base::TimeTicks::Now();
  }

  // Note: If volume_ is currently muted, the |sink_| will not be started yet.
  MaybeStartSink();
}

void WebRtcLocalAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Pause()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  base::AutoLock auto_lock(thread_lock_);
  // Temporarily suspends rendering audio.
  // WebRtcLocalAudioRenderer::Render() will return early during this state
  // and only zeros will be provided to the active sink.
  playing_ = false;
}

void WebRtcLocalAudioRenderer::SetVolume(float volume) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::SetVolume(" << volume << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    // Cache the volume.
    volume_ = volume;
  }

  // Lazily start the |sink_| when the local renderer is unmuted during
  // playing.
  MaybeStartSink();

  if (sink_.get())
    sink_->SetVolume(volume);
}

media::OutputDevice* WebRtcLocalAudioRenderer::GetOutputDevice() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return this;
}

base::TimeDelta WebRtcLocalAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(thread_lock_);
  if (!sink_.get())
    return base::TimeDelta();
  return total_render_time();
}

bool WebRtcLocalAudioRenderer::IsLocalRenderer() const {
  return true;
}

void WebRtcLocalAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const media::SwitchOutputDeviceCB& callback) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::SwitchOutputDevice()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  scoped_refptr<media::AudioOutputDevice> new_sink =
      AudioDeviceFactory::NewOutputDevice(source_render_frame_id_, session_id_,
                                          device_id, security_origin);
  if (new_sink->GetDeviceStatus() != media::OUTPUT_DEVICE_STATUS_OK) {
    callback.Run(new_sink->GetDeviceStatus());
    return;
  }

  output_device_id_ = device_id;
  security_origin_ = security_origin;
  bool was_sink_started = sink_started_;

  if (sink_.get())
    sink_->Stop();

  sink_started_ = false;
  sink_ = new_sink;
  int frames_per_buffer = sink_->GetOutputParameters().frames_per_buffer();
  sink_params_ = source_params_;
  sink_params_.set_frames_per_buffer(WebRtcAudioRenderer::GetOptimalBufferSize(
      source_params_.sample_rate(), frames_per_buffer));

  if (was_sink_started)
    MaybeStartSink();

  callback.Run(media::OUTPUT_DEVICE_STATUS_OK);
}

media::AudioParameters WebRtcLocalAudioRenderer::GetOutputParameters() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!sink_.get())
    return media::AudioParameters();

  return sink_->GetOutputParameters();
}

media::OutputDeviceStatus WebRtcLocalAudioRenderer::GetDeviceStatus() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!sink_.get())
    return media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL;

  return sink_->GetDeviceStatus();
}

void WebRtcLocalAudioRenderer::MaybeStartSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::MaybeStartSink()";

  if (!sink_.get() || !source_params_.IsValid())
    return;

  {
    // Clear up the old data in the FIFO.
    base::AutoLock auto_lock(thread_lock_);
    audio_shifter_->Flush();
  }

  if (!sink_params_.IsValid() || !playing_ || !volume_ || sink_started_ ||
      sink_->GetDeviceStatus() != media::OUTPUT_DEVICE_STATUS_OK)
    return;

  DVLOG(1) << "WebRtcLocalAudioRenderer::MaybeStartSink() -- Starting sink_.";
  sink_->Initialize(sink_params_, this);
  sink_->Start();
  sink_started_ = true;
  UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                            kSinkStarted, kSinkStatesMax);
}

void WebRtcLocalAudioRenderer::ReconfigureSink(
    const media::AudioParameters& params) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "WebRtcLocalAudioRenderer::ReconfigureSink()";

  if (source_params_.Equals(params))
    return;

  // Reset the |source_params_|, |sink_params_| and |loopback_fifo_| to match
  // the new format.

  source_params_ = params;
  {
    // Note: The max buffer is fairly large, but will rarely be used.
    // Cast needs the buffer to hold at least one second of audio.
    // The clock accuracy is set to 20ms because clock accuracy is
    // ~15ms on windows.
    media::AudioShifter* const new_shifter = new media::AudioShifter(
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromMilliseconds(20),
        base::TimeDelta::FromSeconds(20),
        source_params_.sample_rate(),
        params.channels());

    base::AutoLock auto_lock(thread_lock_);
    audio_shifter_.reset(new_shifter);
  }

  if (!sink_.get())
    return;  // WebRtcLocalAudioRenderer has not yet been started.

  // Stop |sink_| and re-create a new one to be initialized with different audio
  // parameters.  Then, invoke MaybeStartSink() to restart everything again.
  sink_->Stop();
  sink_started_ = false;
  sink_ =
      AudioDeviceFactory::NewOutputDevice(source_render_frame_id_, session_id_,
                                          output_device_id_, security_origin_);
  int frames_per_buffer = sink_->GetOutputParameters().frames_per_buffer();
  sink_params_ = source_params_;
  sink_params_.set_frames_per_buffer(WebRtcAudioRenderer::GetOptimalBufferSize(
      source_params_.sample_rate(), frames_per_buffer));
  MaybeStartSink();
}

}  // namespace content
