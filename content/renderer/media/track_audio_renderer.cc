// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/track_audio_renderer.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_audio_track.h"
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

// Translates |num_samples_rendered| into a TimeDelta duration and adds it to
// |prior_elapsed_render_time|.
base::TimeDelta ComputeTotalElapsedRenderTime(
    base::TimeDelta prior_elapsed_render_time,
    int64_t num_samples_rendered,
    int sample_rate) {
  return prior_elapsed_render_time + base::TimeDelta::FromMicroseconds(
      num_samples_rendered * base::Time::kMicrosecondsPerSecond / sample_rate);
}

}  // namespace

// media::AudioRendererSink::RenderCallback implementation
int TrackAudioRenderer::Render(media::AudioBus* audio_bus,
                               uint32_t audio_delay_milliseconds,
                               uint32_t frames_skipped) {
  TRACE_EVENT0("audio", "TrackAudioRenderer::Render");
  base::AutoLock auto_lock(thread_lock_);

  if (!audio_shifter_) {
    audio_bus->Zero();
    return 0;
  }

  // TODO(miu): Plumbing is needed to determine the actual playout timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync.  http://crbug.com/335335
  const base::TimeTicks playout_time =
      base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);
  DVLOG(2) << "Pulling audio out of shifter to be played "
           << audio_delay_milliseconds << " ms from now.";
  audio_shifter_->Pull(audio_bus, playout_time);
  num_samples_rendered_ += audio_bus->frames();
  return audio_bus->frames();
}

void TrackAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
}

// content::MediaStreamAudioSink implementation
void TrackAudioRenderer::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks reference_time) {
  DCHECK(audio_thread_checker_.CalledOnValidThread());
  DCHECK(!reference_time.is_null());

  TRACE_EVENT0("audio", "TrackAudioRenderer::CaptureData");

  base::AutoLock auto_lock(thread_lock_);
  if (!audio_shifter_)
    return;

  scoped_ptr<media::AudioBus> audio_data(
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames()));
  audio_bus.CopyTo(audio_data.get());
  // Note: For remote audio sources, |reference_time| is the local playout time,
  // the ideal point-in-time at which the first audio sample should be played
  // out in the future.  For local sources, |reference_time| is the
  // point-in-time at which the first audio sample was captured in the past.  In
  // either case, AudioShifter will auto-detect and do the right thing when
  // audio is pulled from it.
  audio_shifter_->Push(std::move(audio_data), reference_time);
}

void TrackAudioRenderer::OnSetFormat(const media::AudioParameters& params) {
  DVLOG(1) << "TrackAudioRenderer::OnSetFormat()";
  // If the source is restarted, we might have changed to another capture
  // thread.
  audio_thread_checker_.DetachFromThread();
  DCHECK(audio_thread_checker_.CalledOnValidThread());

  // If the parameters changed, the audio in the AudioShifter is invalid and
  // should be dropped.
  {
    base::AutoLock auto_lock(thread_lock_);
    if (audio_shifter_ &&
        (audio_shifter_->sample_rate() != params.sample_rate() ||
         audio_shifter_->channels() != params.channels())) {
      HaltAudioFlowWhileLockHeld();
    }
  }

  // Post a task on the main render thread to reconfigure the |sink_| with the
  // new format.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&TrackAudioRenderer::ReconfigureSink, this, params));
}

TrackAudioRenderer::TrackAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int playout_render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin)
    : audio_track_(audio_track),
      playout_render_frame_id_(playout_render_frame_id),
      session_id_(session_id),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      num_samples_rendered_(0),
      playing_(false),
      output_device_id_(device_id),
      security_origin_(security_origin),
      volume_(0.0),
      sink_started_(false) {
  DCHECK(MediaStreamAudioTrack::GetTrack(audio_track_));
  DVLOG(1) << "TrackAudioRenderer::TrackAudioRenderer()";
}

TrackAudioRenderer::~TrackAudioRenderer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!sink_.get());
  DVLOG(1) << "TrackAudioRenderer::~TrackAudioRenderer()";
}

void TrackAudioRenderer::Start() {
  DVLOG(1) << "TrackAudioRenderer::Start()";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(playing_, false);

  // We get audio data from |audio_track_|...
  MediaStreamAudioSink::AddToAudioTrack(this, audio_track_);
  // ...and |sink_| will get audio data from us.
  DCHECK(!sink_.get());
  sink_ =
      AudioDeviceFactory::NewOutputDevice(playout_render_frame_id_, session_id_,
                                          output_device_id_, security_origin_);

  base::AutoLock auto_lock(thread_lock_);
  prior_elapsed_render_time_ = base::TimeDelta();
  num_samples_rendered_ = 0;
}

void TrackAudioRenderer::Stop() {
  DVLOG(1) << "TrackAudioRenderer::Stop()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  Pause();

  // Stop the output audio stream, i.e, stop asking for data to render.
  // It is safer to call Stop() on the |sink_| to clean up the resources even
  // when the |sink_| is never started.
  if (sink_.get()) {
    sink_->Stop();
    sink_ = NULL;
  }

  if (!sink_started_ && IsLocalRenderer()) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                              kSinkNeverStarted, kSinkStatesMax);
  }
  sink_started_ = false;

  // Ensure that the capturer stops feeding us with captured audio.
  MediaStreamAudioSink::RemoveFromAudioTrack(this, audio_track_);
}

void TrackAudioRenderer::Play() {
  DVLOG(1) << "TrackAudioRenderer::Play()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  playing_ = true;

  MaybeStartSink();
}

void TrackAudioRenderer::Pause() {
  DVLOG(1) << "TrackAudioRenderer::Pause()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  playing_ = false;

  base::AutoLock auto_lock(thread_lock_);
  HaltAudioFlowWhileLockHeld();
}

void TrackAudioRenderer::SetVolume(float volume) {
  DVLOG(1) << "TrackAudioRenderer::SetVolume(" << volume << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Cache the volume.  Whenever |sink_| is re-created, call SetVolume() with
  // this cached volume.
  volume_ = volume;
  if (sink_.get())
    sink_->SetVolume(volume);
}

media::OutputDevice* TrackAudioRenderer::GetOutputDevice() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return this;
}

base::TimeDelta TrackAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(thread_lock_);
  if (source_params_.IsValid()) {
    return ComputeTotalElapsedRenderTime(prior_elapsed_render_time_,
                                         num_samples_rendered_,
                                         source_params_.sample_rate());
  }
  return prior_elapsed_render_time_;
}

bool TrackAudioRenderer::IsLocalRenderer() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return MediaStreamAudioTrack::GetTrack(audio_track_)->is_local_track();
}

void TrackAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const media::SwitchOutputDeviceCB& callback) {
  DVLOG(1) << "TrackAudioRenderer::SwitchOutputDevice()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    HaltAudioFlowWhileLockHeld();
  }

  scoped_refptr<media::AudioOutputDevice> new_sink =
      AudioDeviceFactory::NewOutputDevice(playout_render_frame_id_, session_id_,
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
  if (was_sink_started)
    MaybeStartSink();

  callback.Run(media::OUTPUT_DEVICE_STATUS_OK);
}

media::AudioParameters TrackAudioRenderer::GetOutputParameters() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!sink_ || !source_params_.IsValid())
    return media::AudioParameters();

  // Output parameters consist of the same channel layout and sample rate as the
  // source, but having the buffer duration preferred by the hardware.
  const media::AudioParameters& preferred_params = sink_->GetOutputParameters();
  return media::AudioParameters(
      preferred_params.format(), source_params_.channel_layout(),
      source_params_.sample_rate(), source_params_.bits_per_sample(),
      preferred_params.frames_per_buffer() * source_params_.sample_rate() /
          preferred_params.sample_rate());
}

media::OutputDeviceStatus TrackAudioRenderer::GetDeviceStatus() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!sink_.get())
    return media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL;

  return sink_->GetDeviceStatus();
}

void TrackAudioRenderer::MaybeStartSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "TrackAudioRenderer::MaybeStartSink()";

  if (!sink_.get() || !source_params_.IsValid() || !playing_)
    return;

  // Re-create the AudioShifter to drop old audio data and reset to a starting
  // state.  MaybeStartSink() is always called in a situation where either the
  // source or sink has changed somehow and so all of AudioShifter's internal
  // time-sync state is invalid.
  CreateAudioShifter();

  if (sink_started_ ||
      sink_->GetDeviceStatus() != media::OUTPUT_DEVICE_STATUS_OK) {
    return;
  }

  DVLOG(1) << ("TrackAudioRenderer::MaybeStartSink() -- Starting sink.  "
               "source_params_={")
           << source_params_.AsHumanReadableString() << "}, sink parameters={"
           << GetOutputParameters().AsHumanReadableString() << '}';
  sink_->Initialize(GetOutputParameters(), this);
  sink_->Start();
  sink_->SetVolume(volume_);
  sink_started_ = true;
  if (IsLocalRenderer()) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates", kSinkStarted,
                              kSinkStatesMax);
  }
}

void TrackAudioRenderer::ReconfigureSink(const media::AudioParameters& params) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "TrackAudioRenderer::ReconfigureSink()";

  if (source_params_.Equals(params))
    return;
  source_params_ = params;

  if (!sink_.get())
    return;  // TrackAudioRenderer has not yet been started.

  // Stop |sink_| and re-create a new one to be initialized with different audio
  // parameters.  Then, invoke MaybeStartSink() to restart everything again.
  sink_->Stop();
  sink_started_ = false;
  sink_ =
      AudioDeviceFactory::NewOutputDevice(playout_render_frame_id_, session_id_,
                                          output_device_id_, security_origin_);
  MaybeStartSink();
}

void TrackAudioRenderer::CreateAudioShifter() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Note 1: The max buffer is fairly large to cover the case where
  // remotely-sourced audio is delivered well ahead of its scheduled playout
  // time (e.g., content streaming with a very large end-to-end
  // latency). However, there is no penalty for making it large in the
  // low-latency use cases since AudioShifter will discard data as soon as it is
  // no longer needed.
  //
  // Note 2: The clock accuracy is set to 20ms because clock accuracy is
  // ~15ms on Windows machines without a working high-resolution clock.  See
  // comments in base/time/time.h for details.
  media::AudioShifter* const new_shifter = new media::AudioShifter(
      base::TimeDelta::FromSeconds(5), base::TimeDelta::FromMilliseconds(20),
      base::TimeDelta::FromSeconds(20), source_params_.sample_rate(),
      source_params_.channels());

  base::AutoLock auto_lock(thread_lock_);
  audio_shifter_.reset(new_shifter);
}

void TrackAudioRenderer::HaltAudioFlowWhileLockHeld() {
  thread_lock_.AssertAcquired();

  audio_shifter_.reset();

  if (source_params_.IsValid()) {
    prior_elapsed_render_time_ =
        ComputeTotalElapsedRenderTime(prior_elapsed_render_time_,
                                      num_samples_rendered_,
                                      source_params_.sample_rate());
    num_samples_rendered_ = 0;
  }
}

}  // namespace content
