// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"

using base::TimeDelta;

namespace media {

PipelineImpl::PipelineImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    MediaLog* media_log)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(media_task_runner),
      media_log_(media_log),
      running_(false),
      did_loading_progress_(false),
      volume_(1.0f),
      playback_rate_(0.0),
      status_(PIPELINE_OK),
      state_(kCreated),
      suspend_timestamp_(kNoTimestamp()),
      renderer_ended_(false),
      text_renderer_ended_(false),
      demuxer_(NULL),
      cdm_context_(nullptr),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(kCreated));
}

PipelineImpl::~PipelineImpl() {
  DCHECK(main_task_runner_->BelongsToCurrentThread())
      << "Pipeline must be destroyed on same thread that created it";
  DCHECK(!running_) << "Stop() must complete before destroying object";
  DCHECK(seek_cb_.is_null());
}

void PipelineImpl::Start(Demuxer* demuxer,
                         std::unique_ptr<Renderer> renderer,
                         Client* client,
                         const PipelineStatusCB& seek_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(client);
  DCHECK(!seek_cb.is_null());

  base::AutoLock auto_lock(lock_);
  CHECK(!running_) << "Media pipeline is already running";
  running_ = true;

  demuxer_ = demuxer;
  renderer_ = std::move(renderer);
  client_weak_factory_.reset(new base::WeakPtrFactory<Client>(client));
  weak_client_ = client_weak_factory_->GetWeakPtr();
  seek_cb_ = media::BindToCurrentLoop(seek_cb);
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::StartTask, weak_this_));
}

void PipelineImpl::Stop() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  if (media_task_runner_ != main_task_runner_) {
    // This path is executed by production code where the two task runners -
    // main and media - live on different threads.
    // TODO(alokp): It may be possible to not have to wait for StopTask by
    // moving the members accessed on media thread into a class/struct and
    // DeleteSoon the instance on the media thread.
    base::WaitableEvent waiter(false, false);
    base::Closure stop_cb =
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter));
    // If posting the task fails or the posted task fails to run,
    // we will wait here forever. So add a CHECK to make sure we do not run
    // into those situations.
    CHECK(weak_factory_.HasWeakPtrs());
    CHECK(media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PipelineImpl::StopTask, weak_this_, stop_cb)));
    waiter.Wait();
  } else {
    // This path is executed by unittests that share media and main threads.
    StopTask(base::Bind(&base::DoNothing));
  }
  // Invalidate client weak pointer effectively canceling all pending client
  // notifications in the message queue.
  client_weak_factory_.reset();
}

void PipelineImpl::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!IsRunning()) {
    DLOG(ERROR) << "Media pipeline isn't running. Ignoring Seek().";
    return;
  }

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::SeekTask, weak_this_, time,
                            media::BindToCurrentLoop(seek_cb)));
}

bool PipelineImpl::IsRunning() const {
  // TODO(alokp): Add thread DCHECK after removing the internal usage on
  // media thread.
  base::AutoLock auto_lock(lock_);
  return running_;
}

double PipelineImpl::GetPlaybackRate() const {
  // TODO(alokp): Add thread DCHECK after removing the internal usage on
  // media thread.
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void PipelineImpl::SetPlaybackRate(double playback_rate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (playback_rate < 0.0)
    return;

  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  if (running_) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PipelineImpl::PlaybackRateChangedTask,
                              weak_this_, playback_rate));
  }
}

void PipelineImpl::Suspend(const PipelineStatusCB& suspend_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::SuspendTask, weak_this_,
                            media::BindToCurrentLoop(suspend_cb)));
}

void PipelineImpl::Resume(std::unique_ptr<Renderer> renderer,
                          base::TimeDelta timestamp,
                          const PipelineStatusCB& seek_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::ResumeTask, weak_this_, base::Passed(&renderer),
                 timestamp, media::BindToCurrentLoop(seek_cb)));
}

float PipelineImpl::GetVolume() const {
  // TODO(alokp): Add thread DCHECK after removing the internal usage on
  // media thread.
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void PipelineImpl::SetVolume(float volume) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (volume < 0.0f || volume > 1.0f)
    return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  if (running_) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PipelineImpl::VolumeChangedTask, weak_this_, volume));
  }
}

TimeDelta PipelineImpl::GetMediaTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (suspend_timestamp_ != kNoTimestamp())
    return suspend_timestamp_;
  return renderer_ ? std::min(renderer_->GetMediaTime(), duration_)
                   : TimeDelta();
}

Ranges<TimeDelta> PipelineImpl::GetBufferedTimeRanges() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  return buffered_time_ranges_;
}

TimeDelta PipelineImpl::GetMediaDuration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  return duration_;
}

bool PipelineImpl::DidLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics PipelineImpl::GetStatistics() const {
  // TODO(alokp): Add thread DCHECK after removing the internal usage on
  // media thread.
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void PipelineImpl::SetCdm(CdmContext* cdm_context,
                          const CdmAttachedCB& cdm_attached_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::SetCdmTask, weak_this_, cdm_context,
                            cdm_attached_cb));
}

void PipelineImpl::SetErrorForTesting(PipelineStatus status) {
  OnError(status);
}

bool PipelineImpl::HasWeakPtrsForTesting() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return weak_factory_.HasWeakPtrs();
}

void PipelineImpl::SetState(State next_state) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << GetStateString(state_) << " -> " << GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(next_state));
}

#define RETURN_STRING(state) \
  case state:                \
    return #state;

const char* PipelineImpl::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kInitDemuxer);
    RETURN_STRING(kInitRenderer);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kPlaying);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
    RETURN_STRING(kSuspending);
    RETURN_STRING(kSuspended);
    RETURN_STRING(kResuming);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

PipelineImpl::State PipelineImpl::GetNextState() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null()) << "State transitions don't happen when stopping";
  DCHECK_EQ(status_, PIPELINE_OK)
      << "State transitions don't happen when there's an error: " << status_;

  switch (state_) {
    case kCreated:
      return kInitDemuxer;

    case kInitDemuxer:
      return kInitRenderer;

    case kInitRenderer:
    case kSeeking:
      return kPlaying;

    case kSuspending:
      return kSuspended;

    case kSuspended:
      return kResuming;

    case kResuming:
      return kPlaying;

    case kPlaying:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}

void PipelineImpl::OnDemuxerError(PipelineStatus error) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::ErrorChangedTask, weak_this_, error));
}

void PipelineImpl::AddTextStream(DemuxerStream* text_stream,
                                 const TextTrackConfig& config) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::AddTextStreamTask, weak_this_,
                            text_stream, config));
}

void PipelineImpl::RemoveTextStream(DemuxerStream* text_stream) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::RemoveTextStreamTask, weak_this_, text_stream));
}

void PipelineImpl::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(IsRunning());
  DCHECK_NE(PIPELINE_OK, error);
  VLOG(1) << "Media pipeline error: " << error;

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::ErrorChangedTask, weak_this_, error));
}

void PipelineImpl::SetDuration(TimeDelta duration) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  DCHECK(IsRunning());
  media_log_->AddEvent(media_log_->CreateTimeEvent(MediaLogEvent::DURATION_SET,
                                                   "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  base::AutoLock auto_lock(lock_);
  duration_ = duration;
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::Client::OnDurationChange, weak_client_));
}

void PipelineImpl::StateTransitionTask(PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // No-op any state transitions if we're stopping.
  if (state_ == kStopping || state_ == kStopped)
    return;

  // Report error from the previous operation.
  if (status != PIPELINE_OK) {
    ErrorChangedTask(status);
    return;
  }

  // Guard against accidentally clearing |pending_callbacks_| for states that
  // use it as well as states that should not be using it.
  DCHECK_EQ(pending_callbacks_.get() != NULL,
            state_ == kSeeking || state_ == kSuspending || state_ == kResuming);

  pending_callbacks_.reset();

  PipelineStatusCB done_cb =
      base::Bind(&PipelineImpl::StateTransitionTask, weak_this_);

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitRenderer:
      // When the state_ transfers to kInitRenderer, it means the demuxer has
      // finished parsing the init info. It should call ReportMetadata in case
      // meeting 'decode' error when passing media segment but WebMediaPlayer's
      // ready_state_ is still ReadyStateHaveNothing. In that case, it will
      // treat it as NetworkStateFormatError not NetworkStateDecodeError.
      ReportMetadata();
      start_timestamp_ = demuxer_->GetStartTime();

      return InitializeRenderer(done_cb);

    case kPlaying:
      DCHECK(start_timestamp_ >= base::TimeDelta());
      renderer_->StartPlayingFrom(start_timestamp_);
      {
        base::AutoLock auto_lock(lock_);
        suspend_timestamp_ = kNoTimestamp();
      }

      if (text_renderer_)
        text_renderer_->StartPlaying();

      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);

      PlaybackRateChangedTask(GetPlaybackRate());
      VolumeChangedTask(GetVolume());
      return;

    case kSuspended:
      renderer_.reset();
      {
        base::AutoLock auto_lock(lock_);
        statistics_.audio_memory_usage = 0;
        statistics_.video_memory_usage = 0;
      }
      base::ResetAndReturn(&suspend_cb_).Run(PIPELINE_OK);
      return;

    case kStopping:
    case kStopped:
    case kCreated:
    case kSeeking:
    case kSuspending:
    case kResuming:
      NOTREACHED() << "State has no transition: " << state_;
      return;
  }
}

// Note that the usage of base::Unretained() with the renderers is considered
// safe as they are owned by |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.
void PipelineImpl::DoSeek(TimeDelta seek_timestamp,
                          const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  DCHECK_EQ(state_, kSeeking);
  SerialRunner::Queue bound_fns;

  // Pause.
  if (text_renderer_) {
    bound_fns.Push(base::Bind(&TextRenderer::Pause,
                              base::Unretained(text_renderer_.get())));
  }

  // Flush.
  DCHECK(renderer_);
  bound_fns.Push(
      base::Bind(&Renderer::Flush, base::Unretained(renderer_.get())));

  if (text_renderer_) {
    bound_fns.Push(base::Bind(&TextRenderer::Flush,
                              base::Unretained(text_renderer_.get())));
  }

  // Seek demuxer.
  bound_fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::DoStop() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  DCHECK(!pending_callbacks_.get());

  // TODO(scherkus): Enforce that Renderer is only called on a single thread,
  // even for accessing media time http://crbug.com/370634
  std::unique_ptr<Renderer> renderer;
  {
    base::AutoLock auto_lock(lock_);
    renderer.swap(renderer_);
  }
  renderer.reset();
  text_renderer_.reset();

  if (demuxer_) {
    demuxer_->Stop();
    demuxer_ = NULL;
  }

  {
    base::AutoLock auto_lock(lock_);
    running_ = false;
  }
  SetState(kStopped);

  // If we stop during initialization/seeking/suspending we don't want to leave
  // outstanding callbacks around. The callbacks also do not get run if the
  // pipeline is stopped before it had a chance to complete outstanding tasks.
  seek_cb_.Reset();
  suspend_cb_.Reset();

  if (!stop_cb_.is_null()) {
    // Invalid all weak pointers so it's safe to destroy |this| on the render
    // main thread.
    weak_factory_.InvalidateWeakPtrs();

    // Post the stop callback to enqueue it after the tasks that may have been
    // Demuxer and Renderer during stopping.
    media_task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&stop_cb_));
  }
}

void PipelineImpl::OnBufferedTimeRangesChanged(
    const Ranges<base::TimeDelta>& ranges) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_ = ranges;
  did_loading_progress_ = true;
}

// Called from any thread.
void PipelineImpl::OnUpdateStatistics(const PipelineStatistics& stats_delta) {
  base::AutoLock auto_lock(lock_);
  statistics_.audio_bytes_decoded += stats_delta.audio_bytes_decoded;
  statistics_.video_bytes_decoded += stats_delta.video_bytes_decoded;
  statistics_.video_frames_decoded += stats_delta.video_frames_decoded;
  statistics_.video_frames_dropped += stats_delta.video_frames_dropped;
  statistics_.audio_memory_usage += stats_delta.audio_memory_usage;
  statistics_.video_memory_usage += stats_delta.video_memory_usage;
}

void PipelineImpl::OnWaitingForDecryptionKey() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::Client::OnWaitingForDecryptionKey, weak_client_));
}

void PipelineImpl::StartTask() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  CHECK_EQ(kCreated, state_)
      << "Media pipeline cannot be started more than once";

  text_renderer_ = CreateTextRenderer();
  if (text_renderer_) {
    text_renderer_->Initialize(
        base::Bind(&PipelineImpl::OnTextRendererEnded, weak_this_));
  }

  StateTransitionTask(PIPELINE_OK);
}

void PipelineImpl::StopTask(const base::Closure& stop_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  if (state_ == kStopped) {
    // Invalid all weak pointers so it's safe to destroy |this| on the render
    // main thread.
    weak_factory_.InvalidateWeakPtrs();

    // NOTE: pipeline may be deleted at this point in time as a result of
    // executing |stop_cb|.
    stop_cb.Run();

    return;
  }

  stop_cb_ = stop_cb;

  // We may already be stopping due to a runtime error.
  if (state_ == kStopping)
    return;

  // Do not report statistics if the pipeline is not fully initialized.
  if (state_ == kSeeking || state_ == kPlaying || state_ == kSuspending ||
      state_ == kSuspended || state_ == kResuming) {
    PipelineStatistics stats = GetStatistics();
    if (stats.video_frames_decoded > 0) {
      UMA_HISTOGRAM_COUNTS("Media.DroppedFrameCount",
                           stats.video_frames_dropped);
    }
  }

  SetState(kStopping);
  pending_callbacks_.reset();
  DoStop();
}

void PipelineImpl::ErrorChangedTask(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  // Preserve existing abnormal status.
  if (status_ != PIPELINE_OK)
    return;

  // Don't report pipeline error events to the media log here. The embedder will
  // log this when Client::OnError is called. If the pipeline is already stopped
  // or stopping we also don't want to log any event. In case we are suspending
  // or suspended, the error may be recoverable, so don't propagate it now,
  // instead let the subsequent seek during resume propagate it if it's
  // unrecoverable.
  if (state_ == kStopping || state_ == kStopped || state_ == kSuspending ||
      state_ == kSuspended) {
    return;
  }

  // Once we enter |kStopping| state, nothing is reported back to the client.
  // If we encounter an error during initialization/seeking/suspending,
  // report the error using the completion callbacks for those tasks.
  status_ = error;
  bool error_reported = false;
  if (!seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(status_);
    error_reported = true;
  }
  if (!suspend_cb_.is_null()) {
    base::ResetAndReturn(&suspend_cb_).Run(status_);
    error_reported = true;
  }
  if (!error_reported) {
    DCHECK_NE(status_, PIPELINE_OK);
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Pipeline::Client::OnError, weak_client_, status_));
  }

  SetState(kStopping);
  pending_callbacks_.reset();
  DoStop();
}

void PipelineImpl::PlaybackRateChangedTask(double playback_rate) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  renderer_->SetPlaybackRate(playback_rate);
}

void PipelineImpl::VolumeChangedTask(float volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Volume changes are only carried out while playing.
  if (state_ != kPlaying)
    return;

  renderer_->SetVolume(volume);
}

void PipelineImpl::SeekTask(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  // Suppress seeking if we're not fully started.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive seek in unexpected state: " << state_;
    seek_cb.Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  DCHECK(seek_cb_.is_null());

  const base::TimeDelta seek_timestamp =
      std::max(time, demuxer_->GetStartTime());

  SetState(kSeeking);
  seek_cb_ = seek_cb;
  renderer_ended_ = false;
  text_renderer_ended_ = false;
  start_timestamp_ = seek_timestamp;

  DoSeek(seek_timestamp,
         base::Bind(&PipelineImpl::StateTransitionTask, weak_this_));
}

void PipelineImpl::SuspendTask(const PipelineStatusCB& suspend_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress suspending if we're not playing.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive suspend in unexpected state: " << state_;
    suspend_cb.Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(renderer_);
  DCHECK(!pending_callbacks_.get());

  SetState(kSuspending);
  suspend_cb_ = suspend_cb;

  // Freeze playback and record the media time before flushing. (Flushing clears
  // the value.)
  renderer_->SetPlaybackRate(0.0);
  {
    base::AutoLock auto_lock(lock_);
    suspend_timestamp_ = renderer_->GetMediaTime();
    DCHECK(suspend_timestamp_ != kNoTimestamp());
  }

  // Queue the asynchronous actions required to stop playback. (Matches setup in
  // DoSeek().)
  // TODO(sandersd): Share implementation with DoSeek().
  SerialRunner::Queue fns;

  if (text_renderer_) {
    fns.Push(base::Bind(&TextRenderer::Pause,
                        base::Unretained(text_renderer_.get())));
  }

  fns.Push(base::Bind(&Renderer::Flush, base::Unretained(renderer_.get())));

  if (text_renderer_) {
    fns.Push(base::Bind(&TextRenderer::Flush,
                        base::Unretained(text_renderer_.get())));
  }

  pending_callbacks_ = SerialRunner::Run(
      fns, base::Bind(&PipelineImpl::StateTransitionTask, weak_this_));
}

void PipelineImpl::ResumeTask(std::unique_ptr<Renderer> renderer,
                              base::TimeDelta timestamp,
                              const PipelineStatusCB& seek_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress resuming if we're not suspended.
  if (state_ != kSuspended) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive resume in unexpected state: " << state_;
    seek_cb.Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!renderer_);
  DCHECK(!pending_callbacks_.get());

  SetState(kResuming);
  renderer_ = std::move(renderer);

  // Set up for a seek. (Matches setup in SeekTask().)
  // TODO(sandersd): Share implementation with SeekTask().
  seek_cb_ = seek_cb;
  renderer_ended_ = false;
  text_renderer_ended_ = false;
  start_timestamp_ = std::max(timestamp, demuxer_->GetStartTime());

  // Queue the asynchronous actions required to start playback. Unlike DoSeek(),
  // we need to initialize the renderer ourselves (we don't want to enter state
  // kInitDemuxer, and even if we did the current code would seek to the start
  // instead of |timestamp|).
  SerialRunner::Queue fns;

  fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), start_timestamp_));

  fns.Push(base::Bind(&PipelineImpl::InitializeRenderer, weak_this_));

  pending_callbacks_ = SerialRunner::Run(
      fns, base::Bind(&PipelineImpl::StateTransitionTask, weak_this_));
}

void PipelineImpl::SetCdmTask(CdmContext* cdm_context,
                              const CdmAttachedCB& cdm_attached_cb) {
  base::AutoLock auto_lock(lock_);
  if (!renderer_) {
    cdm_context_ = cdm_context;
    cdm_attached_cb.Run(true);
    return;
  }

  renderer_->SetCdm(cdm_context,
                    base::Bind(&PipelineImpl::OnCdmAttached, weak_this_,
                               cdm_attached_cb, cdm_context));
}

void PipelineImpl::OnCdmAttached(const CdmAttachedCB& cdm_attached_cb,
                                 CdmContext* cdm_context,
                                 bool success) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (success)
    cdm_context_ = cdm_context;
  cdm_attached_cb.Run(success);
}

void PipelineImpl::OnRendererEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!renderer_ended_);
  renderer_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void PipelineImpl::OnTextRendererEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::TEXT_ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!text_renderer_ended_);
  text_renderer_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void PipelineImpl::RunEndedCallbackIfNeeded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (renderer_ && !renderer_ended_)
    return;

  if (text_renderer_ && text_renderer_->HasTracks() && !text_renderer_ended_)
    return;

  DCHECK_EQ(status_, PIPELINE_OK);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::Client::OnEnded, weak_client_));
}

std::unique_ptr<TextRenderer> PipelineImpl::CreateTextRenderer() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableInbandTextTracks))
    return nullptr;

  return base::WrapUnique(new media::TextRenderer(
      media_task_runner_,
      base::Bind(&PipelineImpl::OnAddTextTrack, weak_this_)));
}

void PipelineImpl::AddTextStreamTask(DemuxerStream* text_stream,
                                     const TextTrackConfig& config) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  // TODO(matthewjheaney): fix up text_ended_ when text stream
  // is added (http://crbug.com/321446).
  if (text_renderer_)
    text_renderer_->AddTextStream(text_stream, config);
}

void PipelineImpl::RemoveTextStreamTask(DemuxerStream* text_stream) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (text_renderer_)
    text_renderer_->RemoveTextStream(text_stream);
}

void PipelineImpl::OnAddTextTrack(const TextTrackConfig& config,
                                  const AddTextTrackDoneCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::Client::OnAddTextTrack, weak_client_,
                            config, done_cb));
}

void PipelineImpl::InitializeDemuxer(const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  demuxer_->Initialize(this, done_cb, !!text_renderer_);
}

void PipelineImpl::InitializeRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!demuxer_->GetStream(DemuxerStream::AUDIO) &&
      !demuxer_->GetStream(DemuxerStream::VIDEO)) {
    {
      base::AutoLock auto_lock(lock_);
      renderer_.reset();
    }
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  if (cdm_context_)
    renderer_->SetCdm(cdm_context_, base::Bind(&IgnoreCdmAttached));

  renderer_->Initialize(
      demuxer_, done_cb,
      base::Bind(&PipelineImpl::OnUpdateStatistics, weak_this_),
      base::Bind(&PipelineImpl::BufferingStateChanged, weak_this_),
      base::Bind(&PipelineImpl::OnRendererEnded, weak_this_),
      base::Bind(&PipelineImpl::OnError, weak_this_),
      base::Bind(&PipelineImpl::OnWaitingForDecryptionKey, weak_this_));
}

void PipelineImpl::ReportMetadata() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  PipelineMetadata metadata;
  metadata.timeline_offset = demuxer_->GetTimelineOffset();
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  if (stream) {
    metadata.has_video = true;
    metadata.natural_size = stream->video_decoder_config().natural_size();
    metadata.video_rotation = stream->video_rotation();
  }
  if (demuxer_->GetStream(DemuxerStream::AUDIO)) {
    metadata.has_audio = true;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::Client::OnMetadata, weak_client_, metadata));
}

void PipelineImpl::BufferingStateChanged(BufferingState new_buffering_state) {
  DVLOG(1) << __FUNCTION__ << "(" << new_buffering_state << ") ";
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Pipeline::Client::OnBufferingStateChange,
                            weak_client_, new_buffering_state));
}

}  // namespace media
