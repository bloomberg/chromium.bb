// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/renderer_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_renderer.h"
#include "media/base/demuxer.h"
#include "media/base/filter_collection.h"
#include "media/base/time_delta_interpolator.h"
#include "media/base/time_source.h"
#include "media/base/video_renderer.h"

namespace media {

RendererImpl::RendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    Demuxer* demuxer,
    scoped_ptr<AudioRenderer> audio_renderer,
    scoped_ptr<VideoRenderer> video_renderer)
    : state_(STATE_UNINITIALIZED),
      task_runner_(task_runner),
      demuxer_(demuxer),
      audio_renderer_(audio_renderer.Pass()),
      video_renderer_(video_renderer.Pass()),
      time_source_(NULL),
      audio_buffering_state_(BUFFERING_HAVE_NOTHING),
      video_buffering_state_(BUFFERING_HAVE_NOTHING),
      audio_ended_(false),
      video_ended_(false),
      underflow_disabled_for_testing_(false),
      interpolator_(new TimeDeltaInterpolator(&default_tick_clock_)),
      interpolation_state_(INTERPOLATION_STOPPED),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()) {
  DVLOG(1) << __FUNCTION__;
  interpolator_->SetBounds(base::TimeDelta(), base::TimeDelta());
}

RendererImpl::~RendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  audio_renderer_.reset();
  video_renderer_.reset();

  FireAllPendingCallbacks();
}

void RendererImpl::Initialize(const PipelineStatusCB& init_cb,
                              const StatisticsCB& statistics_cb,
                              const base::Closure& ended_cb,
                              const PipelineStatusCB& error_cb,
                              const BufferingStateCB& buffering_state_cb,
                              const TimeDeltaCB& get_duration_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED) << state_;
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(!get_duration_cb.is_null());
  DCHECK(demuxer_->GetStream(DemuxerStream::AUDIO) ||
         demuxer_->GetStream(DemuxerStream::VIDEO));

  statistics_cb_ = statistics_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  buffering_state_cb_ = buffering_state_cb;
  get_duration_cb_ = get_duration_cb;

  init_cb_ = init_cb;
  state_ = STATE_INITIALIZING;
  InitializeAudioRenderer();
}

void RendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING) << state_;
  DCHECK(flush_cb_.is_null());

  {
    base::AutoLock auto_lock(interpolator_lock_);
    PauseClockAndStopTicking_Locked();
  }

  flush_cb_ = flush_cb;
  state_ = STATE_FLUSHING;
  FlushAudioRenderer();
}

void RendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING) << state_;

  {
    base::AutoLock auto_lock(interpolator_lock_);
    interpolator_->SetBounds(time, time);
  }

  if (time_source_)
    time_source_->SetMediaTime(time);
  if (audio_renderer_)
    audio_renderer_->StartPlaying();
  if (video_renderer_)
    video_renderer_->StartPlaying();
}

void RendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(2) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != STATE_PLAYING)
    return;

  {
    base::AutoLock auto_lock(interpolator_lock_);
    interpolator_->SetPlaybackRate(playback_rate);
  }

  if (time_source_)
    time_source_->SetPlaybackRate(playback_rate);
}

void RendererImpl::SetVolume(float volume) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

base::TimeDelta RendererImpl::GetMediaTime() {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  base::AutoLock auto_lock(interpolator_lock_);
  return interpolator_->GetInterpolatedTime();
}

bool RendererImpl::HasAudio() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return audio_renderer_ != NULL;
}

bool RendererImpl::HasVideo() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return video_renderer_ != NULL;
}

void RendererImpl::SetCdm(MediaKeys* cdm) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(xhwang): Explore to possibility to move CDM setting from
  // WebMediaPlayerImpl to this class. See http://crbug.com/401264
  NOTREACHED();
}

void RendererImpl::DisableUnderflowForTesting() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  underflow_disabled_for_testing_ = true;
}

void RendererImpl::SetTimeDeltaInterpolatorForTesting(
    TimeDeltaInterpolator* interpolator) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  interpolator_.reset(interpolator);
}

base::TimeDelta RendererImpl::GetMediaDuration() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return get_duration_cb_.Run();
}

void RendererImpl::InitializeAudioRenderer() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnAudioRendererInitializeDone, weak_this_);

  if (!demuxer_->GetStream(DemuxerStream::AUDIO)) {
    audio_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::Bind(done_cb, PIPELINE_OK));
    return;
  }

  audio_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::AUDIO),
      done_cb,
      base::Bind(&RendererImpl::OnUpdateStatistics, weak_this_),
      base::Bind(&RendererImpl::OnAudioTimeUpdate, weak_this_),
      base::Bind(&RendererImpl::OnBufferingStateChanged, weak_this_,
                 &audio_buffering_state_),
      base::Bind(&RendererImpl::OnAudioRendererEnded, weak_this_),
      base::Bind(&RendererImpl::OnError, weak_this_));
}

void RendererImpl::OnAudioRendererInitializeDone(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  if (status != PIPELINE_OK) {
    audio_renderer_.reset();
    state_ = STATE_ERROR;
    base::ResetAndReturn(&init_cb_).Run(status);
    return;
  }

  if (audio_renderer_)
    time_source_ = audio_renderer_->GetTimeSource();

  InitializeVideoRenderer();
}

void RendererImpl::InitializeVideoRenderer() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnVideoRendererInitializeDone, weak_this_);

  if (!demuxer_->GetStream(DemuxerStream::VIDEO)) {
    video_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::Bind(done_cb, PIPELINE_OK));
    return;
  }

  video_renderer_->Initialize(
      demuxer_->GetStream(DemuxerStream::VIDEO),
      demuxer_->GetLiveness() == Demuxer::LIVENESS_LIVE,
      done_cb,
      base::Bind(&RendererImpl::OnUpdateStatistics, weak_this_),
      base::Bind(&RendererImpl::OnVideoTimeUpdate, weak_this_),
      base::Bind(&RendererImpl::OnBufferingStateChanged, weak_this_,
                 &video_buffering_state_),
      base::Bind(&RendererImpl::OnVideoRendererEnded, weak_this_),
      base::Bind(&RendererImpl::OnError, weak_this_),
      base::Bind(&RendererImpl::GetMediaTime, base::Unretained(this)),
      base::Bind(&RendererImpl::GetMediaDuration, base::Unretained(this)));
}

void RendererImpl::OnVideoRendererInitializeDone(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  if (status != PIPELINE_OK) {
    audio_renderer_.reset();
    video_renderer_.reset();
    state_ = STATE_ERROR;
    base::ResetAndReturn(&init_cb_).Run(status);
    return;
  }

  state_ = STATE_PLAYING;
  DCHECK(audio_renderer_ || video_renderer_);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void RendererImpl::FlushAudioRenderer() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING) << state_;
  DCHECK(!flush_cb_.is_null());

  if (!audio_renderer_) {
    OnAudioRendererFlushDone();
    return;
  }

  audio_renderer_->Flush(
      base::Bind(&RendererImpl::OnAudioRendererFlushDone, weak_this_));
}

void RendererImpl::OnAudioRendererFlushDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == STATE_ERROR) {
    DCHECK(flush_cb_.is_null());
    return;
  }

  DCHECK_EQ(state_, STATE_FLUSHING) << state_;
  DCHECK(!flush_cb_.is_null());

  DCHECK_EQ(audio_buffering_state_, BUFFERING_HAVE_NOTHING);
  audio_ended_ = false;
  FlushVideoRenderer();
}

void RendererImpl::FlushVideoRenderer() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING) << state_;
  DCHECK(!flush_cb_.is_null());

  if (!video_renderer_) {
    OnVideoRendererFlushDone();
    return;
  }

  video_renderer_->Flush(
      base::Bind(&RendererImpl::OnVideoRendererFlushDone, weak_this_));
}

void RendererImpl::OnVideoRendererFlushDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == STATE_ERROR) {
    DCHECK(flush_cb_.is_null());
    return;
  }

  DCHECK_EQ(state_, STATE_FLUSHING) << state_;
  DCHECK(!flush_cb_.is_null());

  DCHECK_EQ(video_buffering_state_, BUFFERING_HAVE_NOTHING);
  video_ended_ = false;
  state_ = STATE_PLAYING;
  base::ResetAndReturn(&flush_cb_).Run();
}

void RendererImpl::OnAudioTimeUpdate(base::TimeDelta time,
                                     base::TimeDelta max_time) {
  DVLOG(3) << __FUNCTION__ << "(" << time.InMilliseconds()
           << ", " << max_time.InMilliseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_LE(time.InMicroseconds(), max_time.InMicroseconds());

  base::AutoLock auto_lock(interpolator_lock_);

  if (interpolation_state_ == INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE &&
      time < interpolator_->GetInterpolatedTime()) {
    return;
  }

  if (state_ == STATE_FLUSHING)
    return;

  interpolator_->SetBounds(time, max_time);
  StartClockIfWaitingForTimeUpdate_Locked();
}

void RendererImpl::OnVideoTimeUpdate(base::TimeDelta max_time) {
  DVLOG(3) << __FUNCTION__ << "(" << max_time.InMilliseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_)
    return;

  if (state_ == STATE_FLUSHING)
    return;

  base::AutoLock auto_lock(interpolator_lock_);
  DCHECK_NE(interpolation_state_, INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE);
  interpolator_->SetUpperBound(max_time);
}

void RendererImpl::OnUpdateStatistics(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  statistics_cb_.Run(stats);
}

void RendererImpl::OnBufferingStateChanged(BufferingState* buffering_state,
                                           BufferingState new_buffering_state) {
  DVLOG(2) << __FUNCTION__ << "(" << *buffering_state << ", "
           << new_buffering_state << ") "
           << (buffering_state == &audio_buffering_state_ ? "audio" : "video");
  DCHECK(task_runner_->BelongsToCurrentThread());
  bool was_waiting_for_enough_data = WaitingForEnoughData();

  *buffering_state = new_buffering_state;

  // Disable underflow by ignoring updates that renderers have ran out of data.
  if (state_ == STATE_PLAYING && underflow_disabled_for_testing_ &&
      interpolation_state_ != INTERPOLATION_STOPPED) {
    DVLOG(2) << "Update ignored because underflow is disabled for testing.";
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

bool RendererImpl::WaitingForEnoughData() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != STATE_PLAYING)
    return false;
  if (audio_renderer_ && audio_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  if (video_renderer_ && video_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  return false;
}

void RendererImpl::PausePlayback() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING);
  DCHECK(WaitingForEnoughData());

  base::AutoLock auto_lock(interpolator_lock_);
  PauseClockAndStopTicking_Locked();
}

void RendererImpl::StartPlayback() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING);
  DCHECK_EQ(interpolation_state_, INTERPOLATION_STOPPED);
  DCHECK(!WaitingForEnoughData());

  if (time_source_) {
    // We use audio stream to update the interpolator. So if there is such a
    // stream, we pause the interpolator until we receive a valid time update.
    base::AutoLock auto_lock(interpolator_lock_);
    interpolation_state_ = INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE;
    time_source_->StartTicking();
  } else {
    base::TimeDelta duration = get_duration_cb_.Run();
    base::AutoLock auto_lock(interpolator_lock_);
    interpolation_state_ = INTERPOLATION_STARTED;
    interpolator_->SetUpperBound(duration);
    interpolator_->StartInterpolating();
  }
}

void RendererImpl::PauseClockAndStopTicking_Locked() {
  DVLOG(2) << __FUNCTION__;
  interpolator_lock_.AssertAcquired();
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

void RendererImpl::StartClockIfWaitingForTimeUpdate_Locked() {
  interpolator_lock_.AssertAcquired();
  if (interpolation_state_ != INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE)
    return;

  interpolation_state_ = INTERPOLATION_STARTED;
  interpolator_->StartInterpolating();
}

void RendererImpl::OnAudioRendererEnded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  // Start clock since there is no more audio to trigger clock updates.
  {
    base::TimeDelta duration = get_duration_cb_.Run();
    base::AutoLock auto_lock(interpolator_lock_);
    interpolator_->SetUpperBound(duration);
    StartClockIfWaitingForTimeUpdate_Locked();
  }

  RunEndedCallbackIfNeeded();
}

void RendererImpl::OnVideoRendererEnded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void RendererImpl::RunEndedCallbackIfNeeded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_)
    return;

  if (video_renderer_ && !video_ended_)
    return;

  {
    base::TimeDelta duration = get_duration_cb_.Run();
    base::AutoLock auto_lock(interpolator_lock_);
    PauseClockAndStopTicking_Locked();
    interpolator_->SetBounds(duration, duration);
  }

  ended_cb_.Run();
}

void RendererImpl::OnError(PipelineStatus error) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  state_ = STATE_ERROR;

  // Pipeline will destroy |this| as the result of error.
  base::ResetAndReturn(&error_cb_).Run(error);

  FireAllPendingCallbacks();
}

void RendererImpl::FireAllPendingCallbacks() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();
}

}  // namespace media
