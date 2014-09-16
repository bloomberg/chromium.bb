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
#include "media/base/demuxer_stream_provider.h"
#include "media/base/time_source.h"
#include "media/base/video_renderer.h"
#include "media/base/wall_clock_time_source.h"

namespace media {

RendererImpl::RendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    DemuxerStreamProvider* demuxer_stream_provider,
    scoped_ptr<AudioRenderer> audio_renderer,
    scoped_ptr<VideoRenderer> video_renderer)
    : state_(STATE_UNINITIALIZED),
      task_runner_(task_runner),
      demuxer_stream_provider_(demuxer_stream_provider),
      audio_renderer_(audio_renderer.Pass()),
      video_renderer_(video_renderer.Pass()),
      time_source_(NULL),
      time_ticking_(false),
      audio_buffering_state_(BUFFERING_HAVE_NOTHING),
      video_buffering_state_(BUFFERING_HAVE_NOTHING),
      audio_ended_(false),
      video_ended_(false),
      underflow_disabled_for_testing_(false),
      clockless_video_playback_enabled_for_testing_(false),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()) {
  DVLOG(1) << __FUNCTION__;
}

RendererImpl::~RendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Tear down in opposite order of construction as |video_renderer_| can still
  // need |time_source_| (which can be |audio_renderer_|) to be alive.
  video_renderer_.reset();
  audio_renderer_.reset();

  FireAllPendingCallbacks();
}

void RendererImpl::Initialize(const base::Closure& init_cb,
                              const StatisticsCB& statistics_cb,
                              const base::Closure& ended_cb,
                              const PipelineStatusCB& error_cb,
                              const BufferingStateCB& buffering_state_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED) << state_;
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO) ||
         demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO));

  statistics_cb_ = statistics_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  buffering_state_cb_ = buffering_state_cb;

  init_cb_ = init_cb;
  state_ = STATE_INITIALIZING;
  InitializeAudioRenderer();
}

void RendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING) << state_;
  DCHECK(flush_cb_.is_null());

  flush_cb_ = flush_cb;
  state_ = STATE_FLUSHING;

  if (time_ticking_)
    PausePlayback();

  FlushAudioRenderer();
}

void RendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING) << state_;

  time_source_->SetMediaTime(time);

  if (audio_renderer_)
    audio_renderer_->StartPlaying();
  if (video_renderer_)
    video_renderer_->StartPlayingFrom(time);
}

void RendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != STATE_PLAYING)
    return;

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
  return time_source_->CurrentMediaTime();
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
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  underflow_disabled_for_testing_ = true;
}

void RendererImpl::EnableClocklessVideoPlaybackForTesting() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(underflow_disabled_for_testing_)
      << "Underflow must be disabled for clockless video playback";

  clockless_video_playback_enabled_for_testing_ = true;
}

base::TimeDelta RendererImpl::GetMediaTimeForSyncingVideo() {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  //
  // TODO(scherkus): Currently called from VideoRendererImpl's internal thread,
  // which should go away at some point http://crbug.com/110814
  if (clockless_video_playback_enabled_for_testing_)
    return base::TimeDelta::Max();

  return time_source_->CurrentMediaTimeForSyncingVideo();
}

void RendererImpl::InitializeAudioRenderer() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnAudioRendererInitializeDone, weak_this_);

  if (!demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO)) {
    audio_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::Bind(done_cb, PIPELINE_OK));
    return;
  }

  audio_renderer_->Initialize(
      demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO),
      done_cb,
      base::Bind(&RendererImpl::OnUpdateStatistics, weak_this_),
      base::Bind(&RendererImpl::OnBufferingStateChanged, weak_this_,
                 &audio_buffering_state_),
      base::Bind(&RendererImpl::OnAudioRendererEnded, weak_this_),
      base::Bind(&RendererImpl::OnError, weak_this_));
}

void RendererImpl::OnAudioRendererInitializeDone(PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  if (status != PIPELINE_OK) {
    audio_renderer_.reset();
    OnError(status);
    return;
  }

  InitializeVideoRenderer();
}

void RendererImpl::InitializeVideoRenderer() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnVideoRendererInitializeDone, weak_this_);

  if (!demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO)) {
    video_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::Bind(done_cb, PIPELINE_OK));
    return;
  }

  video_renderer_->Initialize(
      demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO),
      demuxer_stream_provider_->GetLiveness() ==
          DemuxerStreamProvider::LIVENESS_LIVE,
      done_cb,
      base::Bind(&RendererImpl::OnUpdateStatistics, weak_this_),
      base::Bind(&RendererImpl::OnBufferingStateChanged,
                 weak_this_,
                 &video_buffering_state_),
      base::Bind(&RendererImpl::OnVideoRendererEnded, weak_this_),
      base::Bind(&RendererImpl::OnError, weak_this_),
      base::Bind(&RendererImpl::GetMediaTimeForSyncingVideo,
                 base::Unretained(this)));
}

void RendererImpl::OnVideoRendererInitializeDone(PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());

  if (status != PIPELINE_OK) {
    audio_renderer_.reset();
    video_renderer_.reset();
    OnError(status);
    return;
  }

  if (audio_renderer_) {
    time_source_ = audio_renderer_->GetTimeSource();
  } else {
    wall_clock_time_source_.reset(new WallClockTimeSource());
    time_source_ = wall_clock_time_source_.get();
  }

  state_ = STATE_PLAYING;
  DCHECK(time_source_);
  DCHECK(audio_renderer_ || video_renderer_);
  base::ResetAndReturn(&init_cb_).Run();
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

void RendererImpl::OnUpdateStatistics(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  statistics_cb_.Run(stats);
}

void RendererImpl::OnBufferingStateChanged(BufferingState* buffering_state,
                                           BufferingState new_buffering_state) {
  DVLOG(1) << __FUNCTION__ << "(" << *buffering_state << ", "
           << new_buffering_state << ") "
           << (buffering_state == &audio_buffering_state_ ? "audio" : "video");
  DCHECK(task_runner_->BelongsToCurrentThread());
  bool was_waiting_for_enough_data = WaitingForEnoughData();

  *buffering_state = new_buffering_state;

  // Disable underflow by ignoring updates that renderers have ran out of data.
  if (state_ == STATE_PLAYING && underflow_disabled_for_testing_ &&
      time_ticking_) {
    DVLOG(1) << "Update ignored because underflow is disabled for testing.";
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
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(time_ticking_);
  switch (state_) {
    case STATE_PLAYING:
      DCHECK(PlaybackHasEnded() || WaitingForEnoughData())
          << "Playback should only pause due to ending or underflowing";
      break;

    case STATE_FLUSHING:
      // It's OK to pause playback when flushing.
      break;

    case STATE_UNINITIALIZED:
    case STATE_INITIALIZING:
    case STATE_ERROR:
      NOTREACHED() << "Invalid state: " << state_;
      break;
  }

  time_ticking_ = false;
  time_source_->StopTicking();
}

void RendererImpl::StartPlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING);
  DCHECK(!time_ticking_);
  DCHECK(!WaitingForEnoughData());

  time_ticking_ = true;
  time_source_->StartTicking();
}

void RendererImpl::OnAudioRendererEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void RendererImpl::OnVideoRendererEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

bool RendererImpl::PlaybackHasEnded() const {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_)
    return false;

  if (video_renderer_ && !video_ended_)
    return false;

  return true;
}

void RendererImpl::RunEndedCallbackIfNeeded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!PlaybackHasEnded())
    return;

  if (time_ticking_)
    PausePlayback();

  ended_cb_.Run();
}

void RendererImpl::OnError(PipelineStatus error) {
  DVLOG(1) << __FUNCTION__ << "(" << error << ")";
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
    base::ResetAndReturn(&init_cb_).Run();

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();
}

}  // namespace media
