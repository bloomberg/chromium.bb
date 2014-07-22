// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_splicer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/audio_clock.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

namespace {

enum AudioRendererEvent {
  INITIALIZED,
  RENDER_ERROR,
  RENDER_EVENT_MAX = RENDER_ERROR,
};

void HistogramRendererEvent(AudioRendererEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AudioRendererEvents", event, RENDER_EVENT_MAX + 1);
}

}  // namespace

AudioRendererImpl::AudioRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    media::AudioRendererSink* sink,
    ScopedVector<AudioDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    AudioHardwareConfig* hardware_config)
    : task_runner_(task_runner),
      sink_(sink),
      audio_buffer_stream_(new AudioBufferStream(task_runner,
                                                 decoders.Pass(),
                                                 set_decryptor_ready_cb)),
      hardware_config_(hardware_config),
      playback_rate_(0),
      state_(kUninitialized),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      rendering_(false),
      sink_playing_(false),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      weak_factory_(this) {
  audio_buffer_stream_->set_splice_observer(base::Bind(
      &AudioRendererImpl::OnNewSpliceBuffer, weak_factory_.GetWeakPtr()));
  audio_buffer_stream_->set_config_change_observer(base::Bind(
      &AudioRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));
}

AudioRendererImpl::~AudioRendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If Render() is in progress, this call will wait for Render() to finish.
  // After this call, the |sink_| will not call back into |this| anymore.
  sink_->Stop();

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);
}

void AudioRendererImpl::StartTicking() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!rendering_);
  rendering_ = true;

  base::AutoLock auto_lock(lock_);
  // Wait for an eventual call to SetPlaybackRate() to start rendering.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StartRendering_Locked();
}

void AudioRendererImpl::StartRendering_Locked() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(!sink_playing_);
  DCHECK_NE(playback_rate_, 0);
  lock_.AssertAcquired();

  sink_playing_ = true;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Play();
}

void AudioRendererImpl::StopTicking() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(rendering_);
  rendering_ = false;

  base::AutoLock auto_lock(lock_);
  // Rendering should have already been stopped with a zero playback rate.
  if (playback_rate_ == 0) {
    DCHECK(!sink_playing_);
    return;
  }

  StopRendering_Locked();
}

void AudioRendererImpl::StopRendering_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  DCHECK(sink_playing_);
  lock_.AssertAcquired();

  sink_playing_ = false;

  base::AutoUnlock auto_unlock(lock_);
  sink_->Pause();
}

void AudioRendererImpl::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __FUNCTION__ << "(" << time.InMicroseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!rendering_);
  DCHECK_EQ(state_, kFlushed);

  start_timestamp_ = time;
}

base::TimeDelta AudioRendererImpl::CurrentMediaTime() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(scherkus): Finish implementing when ready to switch Pipeline to using
  // TimeSource http://crbug.com/370634
  NOTIMPLEMENTED();

  return base::TimeDelta();
}

TimeSource* AudioRendererImpl::GetTimeSource() {
  return this;
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  DCHECK(flush_cb_.is_null());

  flush_cb_ = callback;
  ChangeState_Locked(kFlushing);

  if (pending_read_)
    return;

  ChangeState_Locked(kFlushed);
  DoFlush_Locked();
}

void AudioRendererImpl::DoFlush_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  DCHECK(!pending_read_);
  DCHECK_EQ(state_, kFlushed);

  audio_buffer_stream_->Reset(base::Bind(&AudioRendererImpl::ResetDecoderDone,
                                         weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::ResetDecoderDone() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(lock_);

    DCHECK_EQ(state_, kFlushed);
    DCHECK(!flush_cb_.is_null());

    audio_clock_.reset(new AudioClock(audio_parameters_.sample_rate()));
    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;

    // Flush() may have been called while underflowed/not fully buffered.
    if (buffering_state_ != BUFFERING_HAVE_NOTHING)
      SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);

    splicer_->Reset();
    if (buffer_converter_)
      buffer_converter_->Reset();
    algorithm_->FlushBuffers();
  }

  // Changes in buffering state are always posted. Flush callback must only be
  // run after buffering state has been set back to nothing.
  task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&flush_cb_));
}

void AudioRendererImpl::StartPlaying() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(!sink_playing_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";

  ChangeState_Locked(kPlaying);
  AttemptRead_Locked();
}

void AudioRendererImpl::Initialize(DemuxerStream* stream,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& time_cb,
                                   const BufferingStateCB& buffering_state_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(sink_);

  state_ = kInitializing;

  init_cb_ = init_cb;
  time_cb_ = time_cb;
  buffering_state_cb_ = buffering_state_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;

  expecting_config_changes_ = stream->SupportsConfigChanges();
  if (!expecting_config_changes_) {
    // The actual buffer size is controlled via the size of the AudioBus
    // provided to Render(), so just choose something reasonable here for looks.
    int buffer_size = stream->audio_decoder_config().samples_per_second() / 100;
    audio_parameters_.Reset(
        AudioParameters::AUDIO_PCM_LOW_LATENCY,
        stream->audio_decoder_config().channel_layout(),
        ChannelLayoutToChannelCount(
            stream->audio_decoder_config().channel_layout()),
        0,
        stream->audio_decoder_config().samples_per_second(),
        stream->audio_decoder_config().bits_per_channel(),
        buffer_size);
    buffer_converter_.reset();
  } else {
    // TODO(rileya): Support hardware config changes
    const AudioParameters& hw_params = hardware_config_->GetOutputConfig();
    audio_parameters_.Reset(
        hw_params.format(),
        // Always use the source's channel layout and channel count to avoid
        // premature downmixing (http://crbug.com/379288), platform specific
        // issues around channel layouts (http://crbug.com/266674), and
        // unnecessary upmixing overhead.
        stream->audio_decoder_config().channel_layout(),
        ChannelLayoutToChannelCount(
            stream->audio_decoder_config().channel_layout()),
        hw_params.input_channels(),
        hw_params.sample_rate(),
        hw_params.bits_per_sample(),
        hardware_config_->GetHighLatencyBufferSize());
  }

  audio_clock_.reset(new AudioClock(audio_parameters_.sample_rate()));

  audio_buffer_stream_->Initialize(
      stream,
      false,
      statistics_cb,
      base::Bind(&AudioRendererImpl::OnAudioBufferStreamInitialized,
                 weak_factory_.GetWeakPtr()));
}

void AudioRendererImpl::OnAudioBufferStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!audio_parameters_.IsValid()) {
    ChangeState_Locked(kUninitialized);
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (expecting_config_changes_)
    buffer_converter_.reset(new AudioBufferConverter(audio_parameters_));
  splicer_.reset(new AudioSplicer(audio_parameters_.sample_rate()));

  // We're all good! Continue initializing the rest of the audio renderer
  // based on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(audio_parameters_);

  ChangeState_Locked(kFlushed);

  HistogramRendererEvent(INITIALIZED);

  {
    base::AutoUnlock auto_unlock(lock_);
    sink_->Initialize(audio_parameters_, this);
    sink_->Start();

    // Some sinks play on start...
    sink_->Pause();
  }

  DCHECK(!sink_playing_);

  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(sink_);
  sink_->SetVolume(volume);
}

void AudioRendererImpl::DecodedAudioReady(
    AudioBufferStream::Status status,
    const scoped_refptr<AudioBuffer>& buffer) {
  DVLOG(2) << __FUNCTION__ << "(" << status << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == AudioBufferStream::ABORTED ||
      status == AudioBufferStream::DEMUXER_READ_ABORTED) {
    HandleAbortedReadOrDecodeError(false);
    return;
  }

  if (status == AudioBufferStream::DECODE_ERROR) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  DCHECK_EQ(status, AudioBufferStream::OK);
  DCHECK(buffer.get());

  if (state_ == kFlushing) {
    ChangeState_Locked(kFlushed);
    DoFlush_Locked();
    return;
  }

  if (expecting_config_changes_) {
    DCHECK(buffer_converter_);
    buffer_converter_->AddInput(buffer);
    while (buffer_converter_->HasNextBuffer()) {
      if (!splicer_->AddInput(buffer_converter_->GetNextBuffer())) {
        HandleAbortedReadOrDecodeError(true);
        return;
      }
    }
  } else {
    if (!splicer_->AddInput(buffer)) {
      HandleAbortedReadOrDecodeError(true);
      return;
    }
  }

  if (!splicer_->HasNextBuffer()) {
    AttemptRead_Locked();
    return;
  }

  bool need_another_buffer = false;
  while (splicer_->HasNextBuffer())
    need_another_buffer = HandleSplicerBuffer_Locked(splicer_->GetNextBuffer());

  if (!need_another_buffer && !CanRead_Locked())
    return;

  AttemptRead_Locked();
}

bool AudioRendererImpl::HandleSplicerBuffer_Locked(
    const scoped_refptr<AudioBuffer>& buffer) {
  lock_.AssertAcquired();
  if (buffer->end_of_stream()) {
    received_end_of_stream_ = true;
  } else {
    if (state_ == kPlaying) {
      if (IsBeforeStartTime(buffer))
        return true;

      // Trim off any additional time before the start timestamp.
      const base::TimeDelta trim_time = start_timestamp_ - buffer->timestamp();
      if (trim_time > base::TimeDelta()) {
        buffer->TrimStart(buffer->frame_count() *
                          (static_cast<double>(trim_time.InMicroseconds()) /
                           buffer->duration().InMicroseconds()));
      }
      // If the entire buffer was trimmed, request a new one.
      if (!buffer->frame_count())
        return true;
    }

    if (state_ != kUninitialized)
      algorithm_->EnqueueBuffer(buffer);
  }

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
      NOTREACHED();
      return false;

    case kFlushed:
      DCHECK(!pending_read_);
      return false;

    case kPlaying:
      if (buffer->end_of_stream() || algorithm_->IsQueueFull()) {
        if (buffering_state_ == BUFFERING_HAVE_NOTHING)
          SetBufferingState_Locked(BUFFERING_HAVE_ENOUGH);
        return false;
      }
      return true;
  }
  return false;
}

void AudioRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void AudioRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!CanRead_Locked())
    return;

  pending_read_ = true;
  audio_buffer_stream_->Read(base::Bind(&AudioRendererImpl::DecodedAudioReady,
                                        weak_factory_.GetWeakPtr()));
}

bool AudioRendererImpl::CanRead_Locked() {
  lock_.AssertAcquired();

  switch (state_) {
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return false;

    case kPlaying:
      break;
  }

  return !pending_read_ && !received_end_of_stream_ &&
      !algorithm_->IsQueueFull();
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(playback_rate, 0);
  DCHECK(sink_);

  base::AutoLock auto_lock(lock_);

  // We have two cases here:
  // Play: current_playback_rate == 0 && playback_rate != 0
  // Pause: current_playback_rate != 0 && playback_rate == 0
  float current_playback_rate = playback_rate_;
  playback_rate_ = playback_rate;

  if (!rendering_)
    return;

  if (current_playback_rate == 0 && playback_rate != 0) {
    StartRendering_Locked();
    return;
  }

  if (current_playback_rate != 0 && playback_rate == 0) {
    StopRendering_Locked();
    return;
  }
}

bool AudioRendererImpl::IsBeforeStartTime(
    const scoped_refptr<AudioBuffer>& buffer) {
  DCHECK_EQ(state_, kPlaying);
  return buffer && !buffer->end_of_stream() &&
         (buffer->timestamp() + buffer->duration()) < start_timestamp_;
}

int AudioRendererImpl::Render(AudioBus* audio_bus,
                              int audio_delay_milliseconds) {
  const int requested_frames = audio_bus->frames();
  base::TimeDelta playback_delay = base::TimeDelta::FromMilliseconds(
      audio_delay_milliseconds);
  const int delay_frames = static_cast<int>(playback_delay.InSecondsF() *
                                            audio_parameters_.sample_rate());
  int frames_written = 0;
  base::Closure time_cb;
  {
    base::AutoLock auto_lock(lock_);

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    if (playback_rate_ == 0) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      audio_clock_->WroteSilence(requested_frames, delay_frames);
      return 0;
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) We've played all known audio data sent to hardware
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise the buffer has data we can send to the device.
    const base::TimeDelta media_timestamp_before_filling =
        audio_clock_->CurrentMediaTimestamp(base::TimeDelta());
    if (algorithm_->frames_buffered() > 0) {
      frames_written =
          algorithm_->FillBuffer(audio_bus, requested_frames, playback_rate_);
      audio_clock_->WroteAudio(
          frames_written, delay_frames, playback_rate_, algorithm_->GetTime());
    }
    audio_clock_->WroteSilence(requested_frames - frames_written, delay_frames);

    if (frames_written == 0) {
      if (received_end_of_stream_ && !rendered_end_of_stream_ &&
          audio_clock_->CurrentMediaTimestamp(base::TimeDelta()) ==
              audio_clock_->last_endpoint_timestamp()) {
        rendered_end_of_stream_ = true;
        ended_cb_.Run();
      } else if (!received_end_of_stream_ && state_ == kPlaying) {
        if (buffering_state_ != BUFFERING_HAVE_NOTHING) {
          algorithm_->IncreaseQueueCapacity();
          SetBufferingState_Locked(BUFFERING_HAVE_NOTHING);
        }
      }
    }

    if (CanRead_Locked()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&AudioRendererImpl::AttemptRead,
                                        weak_factory_.GetWeakPtr()));
    }

    // We only want to execute |time_cb_| if time has progressed and we haven't
    // signaled end of stream yet.
    if (media_timestamp_before_filling !=
            audio_clock_->CurrentMediaTimestamp(base::TimeDelta()) &&
        !rendered_end_of_stream_) {
      time_cb =
          base::Bind(time_cb_,
                     audio_clock_->CurrentMediaTimestamp(base::TimeDelta()),
                     audio_clock_->last_endpoint_timestamp());
    }
  }

  if (!time_cb.is_null())
    task_runner_->PostTask(FROM_HERE, time_cb);

  DCHECK_LE(frames_written, requested_frames);
  return frames_written;
}

void AudioRendererImpl::OnRenderError() {
  // UMA data tells us this happens ~0.01% of the time. Trigger an error instead
  // of trying to gracefully fall back to a fake sink. It's very likely
  // OnRenderError() should be removed and the audio stack handle errors without
  // notifying clients. See http://crbug.com/234708 for details.
  HistogramRendererEvent(RENDER_ERROR);
  error_cb_.Run(PIPELINE_ERROR_DECODE);
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(bool is_decode_error) {
  lock_.AssertAcquired();

  PipelineStatus status = is_decode_error ? PIPELINE_ERROR_DECODE : PIPELINE_OK;
  switch (state_) {
    case kUninitialized:
    case kInitializing:
      NOTREACHED();
      return;
    case kFlushing:
      ChangeState_Locked(kFlushed);
      if (status == PIPELINE_OK) {
        DoFlush_Locked();
        return;
      }

      error_cb_.Run(status);
      base::ResetAndReturn(&flush_cb_).Run();
      return;

    case kFlushed:
    case kPlaying:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      return;
  }
}

void AudioRendererImpl::ChangeState_Locked(State new_state) {
  DVLOG(1) << __FUNCTION__ << " : " << state_ << " -> " << new_state;
  lock_.AssertAcquired();
  state_ = new_state;
}

void AudioRendererImpl::OnNewSpliceBuffer(base::TimeDelta splice_timestamp) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  splicer_->SetSpliceTimestamp(splice_timestamp);
}

void AudioRendererImpl::OnConfigChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(expecting_config_changes_);
  buffer_converter_->ResetTimestampState();
  // Drain flushed buffers from the converter so the AudioSplicer receives all
  // data ahead of any OnNewSpliceBuffer() calls.  Since discontinuities should
  // only appear after config changes, AddInput() should never fail here.
  while (buffer_converter_->HasNextBuffer())
    CHECK(splicer_->AddInput(buffer_converter_->GetNextBuffer()));
}

void AudioRendererImpl::SetBufferingState_Locked(
    BufferingState buffering_state) {
  DVLOG(1) << __FUNCTION__ << " : " << buffering_state_ << " -> "
           << buffering_state;
  DCHECK_NE(buffering_state_, buffering_state);
  lock_.AssertAcquired();
  buffering_state_ = buffering_state;

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(buffering_state_cb_, buffering_state_));
}

}  // namespace media
