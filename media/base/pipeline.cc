// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/clock.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_renderer.h"

using base::TimeDelta;

namespace media {

Pipeline::Pipeline(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner),
      media_log_(media_log),
      running_(false),
      did_loading_progress_(false),
      total_bytes_(0),
      volume_(1.0f),
      playback_rate_(0.0f),
      clock_(new Clock(&default_tick_clock_)),
      waiting_for_clock_update_(false),
      status_(PIPELINE_OK),
      state_(kCreated),
      audio_ended_(false),
      video_ended_(false),
      text_ended_(false),
      audio_disabled_(false),
      demuxer_(NULL),
      creation_time_(default_tick_clock_.NowTicks()) {
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(kCreated));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_CREATED));
}

Pipeline::~Pipeline() {
  DCHECK(thread_checker_.CalledOnValidThread())
      << "Pipeline must be destroyed on same thread that created it";
  DCHECK(!running_) << "Stop() must complete before destroying object";
  DCHECK(stop_cb_.is_null());
  DCHECK(seek_cb_.is_null());

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_DESTROYED));
}

void Pipeline::Start(scoped_ptr<FilterCollection> collection,
                     const base::Closure& ended_cb,
                     const PipelineStatusCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const PipelineMetadataCB& metadata_cb,
                     const base::Closure& preroll_completed_cb,
                     const base::Closure& duration_change_cb) {
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!seek_cb.is_null());
  DCHECK(!metadata_cb.is_null());
  DCHECK(!preroll_completed_cb.is_null());

  base::AutoLock auto_lock(lock_);
  CHECK(!running_) << "Media pipeline is already running";
  running_ = true;

  filter_collection_ = collection.Pass();
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  seek_cb_ = seek_cb;
  metadata_cb_ = metadata_cb;
  preroll_completed_cb_ = preroll_completed_cb;
  duration_change_cb_ = duration_change_cb;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::StartTask, base::Unretained(this)));
}

void Pipeline::Stop(const base::Closure& stop_cb) {
  base::AutoLock auto_lock(lock_);
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::StopTask, base::Unretained(this), stop_cb));
}

void Pipeline::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  base::AutoLock auto_lock(lock_);
  if (!running_) {
    NOTREACHED() << "Media pipeline isn't running";
    return;
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::SeekTask, base::Unretained(this), time, seek_cb));
}

bool Pipeline::IsRunning() const {
  base::AutoLock auto_lock(lock_);
  return running_;
}

float Pipeline::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void Pipeline::SetPlaybackRate(float playback_rate) {
  if (playback_rate < 0.0f)
    return;

  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  if (running_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::PlaybackRateChangedTask, base::Unretained(this),
        playback_rate));
  }
}

float Pipeline::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void Pipeline::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f)
    return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  if (running_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::VolumeChangedTask, base::Unretained(this), volume));
  }
}

TimeDelta Pipeline::GetMediaTime() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Elapsed();
}

Ranges<TimeDelta> Pipeline::GetBufferedTimeRanges() {
  base::AutoLock auto_lock(lock_);
  Ranges<TimeDelta> time_ranges;
  for (size_t i = 0; i < buffered_time_ranges_.size(); ++i) {
    time_ranges.Add(buffered_time_ranges_.start(i),
                    buffered_time_ranges_.end(i));
  }
  if (clock_->Duration() == TimeDelta() || total_bytes_ == 0)
    return time_ranges;
  for (size_t i = 0; i < buffered_byte_ranges_.size(); ++i) {
    TimeDelta start = TimeForByteOffset_Locked(buffered_byte_ranges_.start(i));
    TimeDelta end = TimeForByteOffset_Locked(buffered_byte_ranges_.end(i));
    // Cap approximated buffered time at the length of the video.
    end = std::min(end, clock_->Duration());
    time_ranges.Add(start, end);
  }

  return time_ranges;
}

TimeDelta Pipeline::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Duration();
}

int64 Pipeline::GetTotalBytes() const {
  base::AutoLock auto_lock(lock_);
  return total_bytes_;
}

bool Pipeline::DidLoadingProgress() const {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics Pipeline::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void Pipeline::SetClockForTesting(Clock* clock) {
  clock_.reset(clock);
}

void Pipeline::SetErrorForTesting(PipelineStatus status) {
  SetError(status);
}

void Pipeline::SetState(State next_state) {
  if (state_ != kStarted && next_state == kStarted &&
      !creation_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Media.TimeToPipelineStarted",
                        default_tick_clock_.NowTicks() - creation_time_);
    creation_time_ = base::TimeTicks();
  }

  DVLOG(2) << GetStateString(state_) << " -> " << GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(next_state));
}

#define RETURN_STRING(state) case state: return #state;

const char* Pipeline::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kInitDemuxer);
    RETURN_STRING(kInitAudioRenderer);
    RETURN_STRING(kInitVideoRenderer);
    RETURN_STRING(kInitPrerolling);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kStarting);
    RETURN_STRING(kStarted);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

Pipeline::State Pipeline::GetNextState() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null())
      << "State transitions don't happen when stopping";
  DCHECK_EQ(status_, PIPELINE_OK)
      << "State transitions don't happen when there's an error: " << status_;

  switch (state_) {
    case kCreated:
      return kInitDemuxer;

    case kInitDemuxer:
      if (demuxer_->GetStream(DemuxerStream::AUDIO))
        return kInitAudioRenderer;
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitAudioRenderer:
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitVideoRenderer:
      return kInitPrerolling;

    case kInitPrerolling:
      return kStarting;

    case kSeeking:
      return kStarting;

    case kStarting:
      return kStarted;

    case kStarted:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}

void Pipeline::OnDemuxerError(PipelineStatus error) {
  SetError(error);
}

void Pipeline::AddTextStream(DemuxerStream* text_stream,
                             const TextTrackConfig& config) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
    &Pipeline::AddTextStreamTask, base::Unretained(this),
    text_stream, config));
}

void Pipeline::RemoveTextStream(DemuxerStream* text_stream) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
    &Pipeline::RemoveTextStreamTask, base::Unretained(this),
    text_stream));
}

void Pipeline::SetError(PipelineStatus error) {
  DCHECK(IsRunning());
  DCHECK_NE(PIPELINE_OK, error);
  VLOG(1) << "Media pipeline error: " << error;

  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::ErrorChangedTask, base::Unretained(this), error));

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));
}

void Pipeline::OnAudioDisabled() {
  DCHECK(IsRunning());
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::AudioDisabledTask, base::Unretained(this)));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::AUDIO_RENDERER_DISABLED));
}

void Pipeline::OnAudioTimeUpdate(TimeDelta time, TimeDelta max_time) {
  DCHECK_LE(time.InMicroseconds(), max_time.InMicroseconds());
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);

  if (audio_disabled_)
    return;

  if (waiting_for_clock_update_ && time < clock_->Elapsed())
    return;

  // TODO(scherkus): |state_| should only be accessed on pipeline thread, see
  // http://crbug.com/137973
  if (state_ == kSeeking)
    return;

  clock_->SetTime(time, max_time);
  StartClockIfWaitingForTimeUpdate_Locked();
}

void Pipeline::OnVideoTimeUpdate(TimeDelta max_time) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);

  if (audio_renderer_ && !audio_disabled_)
    return;

  // TODO(scherkus): |state_| should only be accessed on pipeline thread, see
  // http://crbug.com/137973
  if (state_ == kSeeking)
    return;

  DCHECK(!waiting_for_clock_update_);
  clock_->SetMaxTime(max_time);
}

void Pipeline::SetDuration(TimeDelta duration) {
  DCHECK(IsRunning());
  media_log_->AddEvent(
      media_log_->CreateTimeEvent(
          MediaLogEvent::DURATION_SET, "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  base::AutoLock auto_lock(lock_);
  clock_->SetDuration(duration);
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run();
}

void Pipeline::SetTotalBytes(int64 total_bytes) {
  media_log_->AddEvent(
      media_log_->CreateStringEvent(
          MediaLogEvent::TOTAL_BYTES_SET, "total_bytes",
          base::Int64ToString(total_bytes)));
  int64 total_mbytes = total_bytes >> 20;
  if (total_mbytes > kint32max)
    total_mbytes = kint32max;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Media.TotalMBytes", static_cast<int32>(total_mbytes), 1, kint32max, 50);

  base::AutoLock auto_lock(lock_);
  total_bytes_ = total_bytes;
}

TimeDelta Pipeline::TimeForByteOffset_Locked(int64 byte_offset) const {
  lock_.AssertAcquired();
  // Use floating point to avoid potential overflow when using 64 bit integers.
  double time_offset_in_ms = clock_->Duration().InMilliseconds() *
      (static_cast<double>(byte_offset) / total_bytes_);
  TimeDelta time_offset(TimeDelta::FromMilliseconds(
      static_cast<int64>(time_offset_in_ms)));
  // Since the byte->time calculation is approximate, fudge the beginning &
  // ending areas to look better.
  TimeDelta epsilon = clock_->Duration() / 100;
  if (time_offset < epsilon)
    return TimeDelta();
  if (time_offset + epsilon > clock_->Duration())
    return clock_->Duration();
  return time_offset;
}

void Pipeline::OnStateTransition(PipelineStatus status) {
  // Force post to process state transitions after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::StateTransitionTask, base::Unretained(this), status));
}

void Pipeline::StateTransitionTask(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // No-op any state transitions if we're stopping.
  if (state_ == kStopping || state_ == kStopped)
    return;

  // Preserve existing abnormal status, otherwise update based on the result of
  // the previous operation.
  status_ = (status_ != PIPELINE_OK ? status_ : status);

  if (status_ != PIPELINE_OK) {
    ErrorChangedTask(status_);
    return;
  }

  // Guard against accidentally clearing |pending_callbacks_| for states that
  // use it as well as states that should not be using it.
  //
  // TODO(scherkus): Make every state transition use |pending_callbacks_|.
  DCHECK_EQ(pending_callbacks_.get() != NULL,
            (state_ == kInitPrerolling || state_ == kStarting ||
             state_ == kSeeking));
  pending_callbacks_.reset();

  PipelineStatusCB done_cb = base::Bind(
      &Pipeline::OnStateTransition, base::Unretained(this));

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitAudioRenderer:
      return InitializeAudioRenderer(done_cb);

    case kInitVideoRenderer:
      return InitializeVideoRenderer(done_cb);

    case kInitPrerolling:
      filter_collection_.reset();
      {
        base::AutoLock l(lock_);
        // We do not want to start the clock running. We only want to set the
        // base media time so our timestamp calculations will be correct.
        clock_->SetTime(demuxer_->GetStartTime(), demuxer_->GetStartTime());
      }
      if (!audio_renderer_ && !video_renderer_) {
        done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }

      {
        PipelineMetadata metadata;
        metadata.has_audio = audio_renderer_;
        metadata.has_video = video_renderer_;
        DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
        if (stream)
          metadata.natural_size = stream->video_decoder_config().natural_size();
        metadata_cb_.Run(metadata);
      }

      return DoInitialPreroll(done_cb);

    case kStarting:
      return DoPlay(done_cb);

    case kStarted:
      {
        base::AutoLock l(lock_);
        // We use audio stream to update the clock. So if there is such a
        // stream, we pause the clock until we receive a valid timestamp.
        waiting_for_clock_update_ = true;
        if (!audio_renderer_ || audio_disabled_) {
          clock_->SetMaxTime(clock_->Duration());
          StartClockIfWaitingForTimeUpdate_Locked();
        }
      }

      DCHECK(!seek_cb_.is_null());
      DCHECK_EQ(status_, PIPELINE_OK);

      // Fire canplaythrough immediately after playback begins because of
      // crbug.com/106480.
      // TODO(vrk): set ready state to HaveFutureData when bug above is fixed.
      preroll_completed_cb_.Run();
      return base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);

    case kStopping:
    case kStopped:
    case kCreated:
    case kSeeking:
      NOTREACHED() << "State has no transition: " << state_;
      return;
  }
}

// Note that the usage of base::Unretained() with the audio/video renderers
// in the following DoXXX() functions is considered safe as they are owned by
// |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.
void Pipeline::DoInitialPreroll(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  base::TimeDelta seek_timestamp = demuxer_->GetStartTime();

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, base::Unretained(audio_renderer_.get()),
        seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, base::Unretained(video_renderer_.get()),
        seek_timestamp));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoSeek(
    base::TimeDelta seek_timestamp,
    const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  // Pause.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Pause, base::Unretained(audio_renderer_.get())));
  }
  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Pause, base::Unretained(video_renderer_.get())));
  }
  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Pause, base::Unretained(text_renderer_.get())));
  }

  // Flush.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Flush, base::Unretained(audio_renderer_.get())));
  }
  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Flush, base::Unretained(video_renderer_.get())));
  }
  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Flush, base::Unretained(text_renderer_.get())));
  }

  // Seek demuxer.
  bound_fns.Push(base::Bind(
      &Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, base::Unretained(audio_renderer_.get()),
        seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, base::Unretained(video_renderer_.get()),
        seek_timestamp));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoPlay(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  PlaybackRateChangedTask(GetPlaybackRate());
  VolumeChangedTask(GetVolume());

  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Play, base::Unretained(audio_renderer_.get())));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Play, base::Unretained(video_renderer_.get())));
  }

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Play, base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoStop(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  if (demuxer_) {
    bound_fns.Push(base::Bind(
        &Demuxer::Stop, base::Unretained(demuxer_)));
  }

  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Stop, base::Unretained(audio_renderer_.get())));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Stop, base::Unretained(video_renderer_.get())));
  }

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Stop, base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::OnStopCompleted(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  {
    base::AutoLock l(lock_);
    running_ = false;
  }

  SetState(kStopped);
  pending_callbacks_.reset();
  filter_collection_.reset();
  audio_renderer_.reset();
  video_renderer_.reset();
  text_renderer_.reset();
  demuxer_ = NULL;

  // If we stop during initialization/seeking we want to run |seek_cb_|
  // followed by |stop_cb_| so we don't leave outstanding callbacks around.
  if (!seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(status_);
    error_cb_.Reset();
  }
  if (!stop_cb_.is_null()) {
    error_cb_.Reset();
    base::ResetAndReturn(&stop_cb_).Run();

    // NOTE: pipeline may be deleted at this point in time as a result of
    // executing |stop_cb_|.
    return;
  }
  if (!error_cb_.is_null()) {
    DCHECK_NE(status_, PIPELINE_OK);
    base::ResetAndReturn(&error_cb_).Run(status_);
  }
}

void Pipeline::AddBufferedByteRange(int64 start, int64 end) {
  base::AutoLock auto_lock(lock_);
  buffered_byte_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void Pipeline::AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void Pipeline::OnAudioRendererEnded() {
  // Force post to process ended tasks after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoAudioRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::AUDIO_ENDED));
}

void Pipeline::OnVideoRendererEnded() {
  // Force post to process ended tasks after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoVideoRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::VIDEO_ENDED));
}

void Pipeline::OnTextRendererEnded() {
  // Force post to process ended messages after current execution frame.
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::DoTextRendererEnded, base::Unretained(this)));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::TEXT_ENDED));
}

// Called from any thread.
void Pipeline::OnUpdateStatistics(const PipelineStatistics& stats) {
  base::AutoLock auto_lock(lock_);
  statistics_.audio_bytes_decoded += stats.audio_bytes_decoded;
  statistics_.video_bytes_decoded += stats.video_bytes_decoded;
  statistics_.video_frames_decoded += stats.video_frames_decoded;
  statistics_.video_frames_dropped += stats.video_frames_dropped;
}

void Pipeline::StartTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK_EQ(kCreated, state_)
      << "Media pipeline cannot be started more than once";

  text_renderer_ = filter_collection_->GetTextRenderer();

  if (text_renderer_) {
    text_renderer_->Initialize(
        base::Bind(&Pipeline::OnTextRendererEnded, base::Unretained(this)));
  }

  StateTransitionTask(PIPELINE_OK);
}

void Pipeline::StopTask(const base::Closure& stop_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  if (state_ == kStopped) {
    stop_cb.Run();
    return;
  }

  stop_cb_ = stop_cb;

  // We may already be stopping due to a runtime error.
  if (state_ == kStopping)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  DoStop(base::Bind(&Pipeline::OnStopCompleted, base::Unretained(this)));
}

void Pipeline::ErrorChangedTask(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  if (state_ == kStopping || state_ == kStopped)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  status_ = error;

  DoStop(base::Bind(&Pipeline::OnStopCompleted, base::Unretained(this)));
}

void Pipeline::PlaybackRateChangedTask(float playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != kStarting && state_ != kStarted)
    return;

  {
    base::AutoLock auto_lock(lock_);
    clock_->SetPlaybackRate(playback_rate);
  }

  if (audio_renderer_)
    audio_renderer_->SetPlaybackRate(playback_rate_);
  if (video_renderer_)
    video_renderer_->SetPlaybackRate(playback_rate_);
}

void Pipeline::VolumeChangedTask(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Volume changes are only carried out while playing.
  if (state_ != kStarting && state_ != kStarted)
    return;

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

void Pipeline::SeekTask(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  // Suppress seeking if we're not fully started.
  if (state_ != kStarted) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive extra seek in unexpected state: " << state_;

    // TODO(scherkus): should we run the callback?  I'm tempted to say the API
    // will only execute the first Seek() request.
    DVLOG(1) << "Media pipeline has not started, ignoring seek to "
             << time.InMicroseconds() << " (current state: " << state_ << ")";
    return;
  }

  DCHECK(seek_cb_.is_null());

  SetState(kSeeking);
  base::TimeDelta seek_timestamp = std::max(time, demuxer_->GetStartTime());
  seek_cb_ = seek_cb;
  audio_ended_ = false;
  video_ended_ = false;
  text_ended_ = false;

  // Kick off seeking!
  {
    base::AutoLock auto_lock(lock_);
    if (clock_->IsPlaying())
      clock_->Pause();
    clock_->SetTime(seek_timestamp, seek_timestamp);
  }
  DoSeek(seek_timestamp, base::Bind(
      &Pipeline::OnStateTransition, base::Unretained(this)));
}

void Pipeline::DoAudioRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kStarted)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  // Start clock since there is no more audio to trigger clock updates.
  if (!audio_disabled_) {
    base::AutoLock auto_lock(lock_);
    clock_->SetMaxTime(clock_->Duration());
    StartClockIfWaitingForTimeUpdate_Locked();
  }

  RunEndedCallbackIfNeeded();
}

void Pipeline::DoVideoRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kStarted)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::DoTextRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kStarted)
    return;

  DCHECK(!text_ended_);
  text_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::RunEndedCallbackIfNeeded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_ && !audio_disabled_)
    return;

  if (video_renderer_ && !video_ended_)
    return;

  if (text_renderer_ && text_renderer_->HasTracks() && !text_ended_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    clock_->EndOfStream();
  }

  DCHECK_EQ(status_, PIPELINE_OK);
  ended_cb_.Run();
}

void Pipeline::AudioDisabledTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  audio_disabled_ = true;

  // Notify our demuxer that we're no longer rendering audio.
  demuxer_->OnAudioRendererDisabled();

  // Start clock since there is no more audio to trigger clock updates.
  clock_->SetMaxTime(clock_->Duration());
  StartClockIfWaitingForTimeUpdate_Locked();
}

void Pipeline::AddTextStreamTask(DemuxerStream* text_stream,
                                 const TextTrackConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(matthewjheaney): fix up text_ended_ when text stream
  // is added (http://crbug.com/321446).
  text_renderer_->AddTextStream(text_stream, config);
}

void Pipeline::RemoveTextStreamTask(DemuxerStream* text_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  text_renderer_->RemoveTextStream(text_stream);
}

void Pipeline::InitializeDemuxer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  demuxer_ = filter_collection_->GetDemuxer();
  demuxer_->Initialize(this, done_cb, text_renderer_);
}

void Pipeline::InitializeAudioRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  audio_renderer_ = filter_collection_->GetAudioRenderer();
  audio_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::AUDIO),
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioUnderflow, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioTimeUpdate, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioRendererEnded, base::Unretained(this)),
      base::Bind(&Pipeline::OnAudioDisabled, base::Unretained(this)),
      base::Bind(&Pipeline::SetError, base::Unretained(this)));
}

void Pipeline::InitializeVideoRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  video_renderer_ = filter_collection_->GetVideoRenderer();
  video_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::VIDEO),
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, base::Unretained(this)),
      base::Bind(&Pipeline::OnVideoTimeUpdate, base::Unretained(this)),
      base::Bind(&Pipeline::OnVideoRendererEnded, base::Unretained(this)),
      base::Bind(&Pipeline::SetError, base::Unretained(this)),
      base::Bind(&Pipeline::GetMediaTime, base::Unretained(this)),
      base::Bind(&Pipeline::GetMediaDuration, base::Unretained(this)));
}

void Pipeline::OnAudioUnderflow() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &Pipeline::OnAudioUnderflow, base::Unretained(this)));
    return;
  }

  if (state_ != kStarted)
    return;

  if (audio_renderer_)
    audio_renderer_->ResumeAfterUnderflow();
}

void Pipeline::StartClockIfWaitingForTimeUpdate_Locked() {
  lock_.AssertAcquired();
  if (!waiting_for_clock_update_)
    return;

  waiting_for_clock_update_ = false;
  clock_->Play();
}

}  // namespace media
