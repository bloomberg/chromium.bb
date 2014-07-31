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
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/time_delta_interpolator.h"
#include "media/base/time_source.h"
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
      volume_(1.0f),
      playback_rate_(0.0f),
      interpolator_(new TimeDeltaInterpolator(&default_tick_clock_)),
      interpolation_state_(INTERPOLATION_STOPPED),
      status_(PIPELINE_OK),
      state_(kCreated),
      audio_ended_(false),
      video_ended_(false),
      text_ended_(false),
      audio_buffering_state_(BUFFERING_HAVE_NOTHING),
      video_buffering_state_(BUFFERING_HAVE_NOTHING),
      demuxer_(NULL),
      time_source_(NULL),
      underflow_disabled_for_testing_(false),
      weak_factory_(this) {
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(kCreated));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_CREATED));
  interpolator_->SetBounds(base::TimeDelta(), base::TimeDelta());
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

// The base::Unretained(this) in these functions are safe because:
// 1, No public methods (except for the dtor) should be called after Stop().
// 2, |this| will not be destructed until the stop callback is fired.
// 3, Stop() also posts StopTask(), hence all XxxTask() will be executed before
//    StopTask(), and therefore before the dtor.

void Pipeline::Start(scoped_ptr<FilterCollection> collection,
                     const base::Closure& ended_cb,
                     const PipelineStatusCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const PipelineMetadataCB& metadata_cb,
                     const BufferingStateCB& buffering_state_cb,
                     const base::Closure& duration_change_cb) {
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!seek_cb.is_null());
  DCHECK(!metadata_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());

  base::AutoLock auto_lock(lock_);
  CHECK(!running_) << "Media pipeline is already running";
  running_ = true;

  filter_collection_ = collection.Pass();
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  seek_cb_ = seek_cb;
  metadata_cb_ = metadata_cb;
  buffering_state_cb_ = buffering_state_cb;
  duration_change_cb_ = duration_change_cb;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::StartTask, base::Unretained(this)));
}

void Pipeline::Stop(const base::Closure& stop_cb) {
  DVLOG(2) << __FUNCTION__;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::StopTask, base::Unretained(this), stop_cb));
}

void Pipeline::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  base::AutoLock auto_lock(lock_);
  if (!running_) {
    DLOG(ERROR) << "Media pipeline isn't running. Ignoring Seek().";
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::SeekTask, base::Unretained(this), time, seek_cb));
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
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&Pipeline::PlaybackRateChangedTask,
                                      base::Unretained(this),
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
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &Pipeline::VolumeChangedTask, base::Unretained(this), volume));
  }
}

TimeDelta Pipeline::GetMediaTime() const {
  base::AutoLock auto_lock(lock_);
  return std::min(interpolator_->GetInterpolatedTime(), duration_);
}

Ranges<TimeDelta> Pipeline::GetBufferedTimeRanges() const {
  base::AutoLock auto_lock(lock_);
  return buffered_time_ranges_;
}

TimeDelta Pipeline::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return duration_;
}

bool Pipeline::DidLoadingProgress() {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics Pipeline::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void Pipeline::SetTimeDeltaInterpolatorForTesting(
    TimeDeltaInterpolator* interpolator) {
  interpolator_.reset(interpolator);
}

void Pipeline::SetErrorForTesting(PipelineStatus status) {
  OnError(status);
}

void Pipeline::SetState(State next_state) {
  DVLOG(1) << GetStateString(state_) << " -> " << GetStateString(next_state);

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
    RETURN_STRING(kSeeking);
    RETURN_STRING(kPlaying);
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
      return kPlaying;

    case kInitAudioRenderer:
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kPlaying;

    case kInitVideoRenderer:
      return kPlaying;

    case kSeeking:
      return kPlaying;

    case kPlaying:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}

// The use of base::Unretained(this) in the following 3 functions is safe
// because these functions are called by the Demuxer directly, before the stop
// callback is posted by the Demuxer. So the posted tasks will always be
// executed before the stop callback is executed, and hence before the Pipeline
// is destructed.

void Pipeline::OnDemuxerError(PipelineStatus error) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Pipeline::ErrorChangedTask,
                                    base::Unretained(this),
                                    error));
}

void Pipeline::AddTextStream(DemuxerStream* text_stream,
                             const TextTrackConfig& config) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Pipeline::AddTextStreamTask,
                                    base::Unretained(this),
                                    text_stream,
                                    config));
}

void Pipeline::RemoveTextStream(DemuxerStream* text_stream) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Pipeline::RemoveTextStreamTask,
                                    base::Unretained(this),
                                    text_stream));
}

void Pipeline::OnError(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(IsRunning());
  DCHECK_NE(PIPELINE_OK, error);
  VLOG(1) << "Media pipeline error: " << error;

  task_runner_->PostTask(FROM_HERE, base::Bind(
      &Pipeline::ErrorChangedTask, weak_factory_.GetWeakPtr(), error));
}

void Pipeline::OnAudioTimeUpdate(TimeDelta time, TimeDelta max_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_LE(time.InMicroseconds(), max_time.InMicroseconds());
  base::AutoLock auto_lock(lock_);

  if (interpolation_state_ == INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE &&
      time < interpolator_->GetInterpolatedTime()) {
    return;
  }

  if (state_ == kSeeking)
    return;

  interpolator_->SetBounds(time, max_time);
  StartClockIfWaitingForTimeUpdate_Locked();
}

void Pipeline::OnVideoTimeUpdate(TimeDelta max_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_)
    return;

  if (state_ == kSeeking)
    return;

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(interpolation_state_, INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE);
  interpolator_->SetUpperBound(max_time);
}

void Pipeline::SetDuration(TimeDelta duration) {
  DCHECK(IsRunning());
  media_log_->AddEvent(
      media_log_->CreateTimeEvent(
          MediaLogEvent::DURATION_SET, "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  base::AutoLock auto_lock(lock_);
  duration_ = duration;
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run();
}

void Pipeline::OnStateTransition(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Force post to process state transitions after current execution frame.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &Pipeline::StateTransitionTask, weak_factory_.GetWeakPtr(), status));
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
  DCHECK_EQ(pending_callbacks_.get() != NULL, state_ == kSeeking);

  pending_callbacks_.reset();

  PipelineStatusCB done_cb =
      base::Bind(&Pipeline::OnStateTransition, weak_factory_.GetWeakPtr());

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitAudioRenderer:
      return InitializeAudioRenderer(done_cb);

    case kInitVideoRenderer:
      return InitializeVideoRenderer(done_cb);

    case kPlaying:
      // Finish initial start sequence the first time we enter the playing
      // state.
      if (filter_collection_) {
        filter_collection_.reset();
        if (!audio_renderer_ && !video_renderer_) {
          ErrorChangedTask(PIPELINE_ERROR_COULD_NOT_RENDER);
          return;
        }

        if (audio_renderer_)
          time_source_ = audio_renderer_->GetTimeSource();

        {
          PipelineMetadata metadata;
          metadata.has_audio = audio_renderer_;
          metadata.has_video = video_renderer_;
          metadata.timeline_offset = demuxer_->GetTimelineOffset();
          DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
          if (stream) {
            metadata.natural_size =
                stream->video_decoder_config().natural_size();
            metadata.video_rotation = stream->video_rotation();
          }
          metadata_cb_.Run(metadata);
        }
      }

      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);

      {
        base::AutoLock auto_lock(lock_);
        interpolator_->SetBounds(start_timestamp_, start_timestamp_);
      }

      if (time_source_)
        time_source_->SetMediaTime(start_timestamp_);
      if (audio_renderer_)
        audio_renderer_->StartPlaying();
      if (video_renderer_)
        video_renderer_->StartPlaying();
      if (text_renderer_)
        text_renderer_->StartPlaying();

      PlaybackRateChangedTask(GetPlaybackRate());
      VolumeChangedTask(GetVolume());
      return;

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

#if DCHECK_IS_ON
static void VerifyBufferingStates(BufferingState* audio_buffering_state,
                                  BufferingState* video_buffering_state) {
  DCHECK_EQ(*audio_buffering_state, BUFFERING_HAVE_NOTHING);
  DCHECK_EQ(*video_buffering_state, BUFFERING_HAVE_NOTHING);
}
#endif

void Pipeline::DoSeek(
    base::TimeDelta seek_timestamp,
    const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;
  {
    base::AutoLock auto_lock(lock_);
    PauseClockAndStopTicking_Locked();
  }

  // Pause.
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

#if DCHECK_IS_ON
  // Verify renderers reset their buffering states.
  bound_fns.Push(base::Bind(&VerifyBufferingStates,
                            &audio_buffering_state_,
                            &video_buffering_state_));
#endif

  if (text_renderer_) {
    bound_fns.Push(base::Bind(
        &TextRenderer::Flush, base::Unretained(text_renderer_.get())));
  }

  // Seek demuxer.
  bound_fns.Push(base::Bind(
      &Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void Pipeline::DoStop(const PipelineStatusCB& done_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());

  audio_renderer_.reset();
  video_renderer_.reset();
  text_renderer_.reset();

  if (demuxer_) {
    demuxer_->Stop(base::Bind(done_cb, PIPELINE_OK));
    return;
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(done_cb, PIPELINE_OK));
}

void Pipeline::OnStopCompleted(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  DCHECK(!audio_renderer_);
  DCHECK(!video_renderer_);
  DCHECK(!text_renderer_);
  {
    base::AutoLock l(lock_);
    running_ = false;
  }

  SetState(kStopped);
  filter_collection_.reset();
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

void Pipeline::AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_.Add(start, end);
  did_loading_progress_ = true;
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
        base::Bind(&Pipeline::OnTextRendererEnded, weak_factory_.GetWeakPtr()));
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
  DoStop(base::Bind(&Pipeline::OnStopCompleted, weak_factory_.GetWeakPtr()));
}

void Pipeline::ErrorChangedTask(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));

  if (state_ == kStopping || state_ == kStopped)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  status_ = error;

  DoStop(base::Bind(&Pipeline::OnStopCompleted, weak_factory_.GetWeakPtr()));
}

void Pipeline::PlaybackRateChangedTask(float playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  {
    base::AutoLock auto_lock(lock_);
    interpolator_->SetPlaybackRate(playback_rate);
  }

  if (time_source_)
    time_source_->SetPlaybackRate(playback_rate_);
}

void Pipeline::VolumeChangedTask(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Volume changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

void Pipeline::SeekTask(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  // Suppress seeking if we're not fully started.
  if (state_ != kPlaying) {
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
  seek_cb_ = seek_cb;
  audio_ended_ = false;
  video_ended_ = false;
  text_ended_ = false;
  start_timestamp_ = time;

  DoSeek(time,
         base::Bind(&Pipeline::OnStateTransition, weak_factory_.GetWeakPtr()));
}

void Pipeline::OnAudioRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::AUDIO_ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  // Start clock since there is no more audio to trigger clock updates.
  {
    base::AutoLock auto_lock(lock_);
    interpolator_->SetUpperBound(duration_);
    StartClockIfWaitingForTimeUpdate_Locked();
  }

  RunEndedCallbackIfNeeded();
}

void Pipeline::OnVideoRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::VIDEO_ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::OnTextRendererEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::TEXT_ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!text_ended_);
  text_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void Pipeline::RunEndedCallbackIfNeeded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_)
    return;

  if (video_renderer_ && !video_ended_)
    return;

  if (text_renderer_ && text_renderer_->HasTracks() && !text_ended_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    PauseClockAndStopTicking_Locked();
    interpolator_->SetBounds(duration_, duration_);
  }

  DCHECK_EQ(status_, PIPELINE_OK);
  ended_cb_.Run();
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
  base::WeakPtr<Pipeline> weak_this = weak_factory_.GetWeakPtr();
  audio_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::AUDIO),
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, weak_this),
      base::Bind(&Pipeline::OnAudioTimeUpdate, weak_this),
      base::Bind(&Pipeline::BufferingStateChanged, weak_this,
                 &audio_buffering_state_),
      base::Bind(&Pipeline::OnAudioRendererEnded, weak_this),
      base::Bind(&Pipeline::OnError, weak_this));
}

void Pipeline::InitializeVideoRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  video_renderer_ = filter_collection_->GetVideoRenderer();
  base::WeakPtr<Pipeline> weak_this = weak_factory_.GetWeakPtr();
  video_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::VIDEO),
      demuxer_->GetLiveness() == Demuxer::LIVENESS_LIVE,
      done_cb,
      base::Bind(&Pipeline::OnUpdateStatistics, weak_this),
      base::Bind(&Pipeline::OnVideoTimeUpdate, weak_this),
      base::Bind(&Pipeline::BufferingStateChanged, weak_this,
                 &video_buffering_state_),
      base::Bind(&Pipeline::OnVideoRendererEnded, weak_this),
      base::Bind(&Pipeline::OnError, weak_this),
      base::Bind(&Pipeline::GetMediaTime, base::Unretained(this)),
      base::Bind(&Pipeline::GetMediaDuration, base::Unretained(this)));
}

void Pipeline::BufferingStateChanged(BufferingState* buffering_state,
                                     BufferingState new_buffering_state) {
  DVLOG(1) << __FUNCTION__ << "(" << *buffering_state << ", "
           << " " << new_buffering_state << ") "
           << (buffering_state == &audio_buffering_state_ ? "audio" : "video");
  DCHECK(task_runner_->BelongsToCurrentThread());
  bool was_waiting_for_enough_data = WaitingForEnoughData();

  *buffering_state = new_buffering_state;

  // Disable underflow by ignoring updates that renderers have ran out of data
  // after we have started the clock.
  if (state_ == kPlaying && underflow_disabled_for_testing_ &&
      interpolation_state_ != INTERPOLATION_STOPPED) {
    return;
  }

  // Renderer underflowed.
  if (!was_waiting_for_enough_data && WaitingForEnoughData()) {
    PausePlayback();

    // TODO(scherkus): Fire BUFFERING_HAVE_NOTHING callback to alert clients of
    // underflow state http://crbug.com/144683
    return;
  }

  // Renderer prerolled.
  if (was_waiting_for_enough_data && !WaitingForEnoughData()) {
    StartPlayback();
    buffering_state_cb_.Run(BUFFERING_HAVE_ENOUGH);
    return;
  }
}

bool Pipeline::WaitingForEnoughData() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kPlaying)
    return false;
  if (audio_renderer_ && audio_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  if (video_renderer_ && video_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  return false;
}

void Pipeline::PausePlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, kPlaying);
  DCHECK(WaitingForEnoughData());
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  PauseClockAndStopTicking_Locked();
}

void Pipeline::StartPlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, kPlaying);
  DCHECK_EQ(interpolation_state_, INTERPOLATION_STOPPED);
  DCHECK(!WaitingForEnoughData());
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (time_source_) {
    // We use audio stream to update the clock. So if there is such a
    // stream, we pause the clock until we receive a valid timestamp.
    base::AutoLock auto_lock(lock_);
    interpolation_state_ = INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE;
    time_source_->StartTicking();
  } else {
    base::AutoLock auto_lock(lock_);
    interpolation_state_ = INTERPOLATION_STARTED;
    interpolator_->SetUpperBound(duration_);
    interpolator_->StartInterpolating();
  }
}

void Pipeline::PauseClockAndStopTicking_Locked() {
  lock_.AssertAcquired();
  switch (interpolation_state_) {
    case INTERPOLATION_STOPPED:
      return;

    case INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE:
      time_source_->StopTicking();
      break;

    case INTERPOLATION_STARTED:
      if (time_source_)
        time_source_->StopTicking();
      interpolator_->StopInterpolating();
      break;
  }

  interpolation_state_ = INTERPOLATION_STOPPED;
}

void Pipeline::StartClockIfWaitingForTimeUpdate_Locked() {
  lock_.AssertAcquired();
  if (interpolation_state_ != INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE)
    return;

  interpolation_state_ = INTERPOLATION_STARTED;
  interpolator_->StartInterpolating();
}

}  // namespace media
