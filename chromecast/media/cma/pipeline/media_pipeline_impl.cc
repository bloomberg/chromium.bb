// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "chromecast/media/cma/base/buffering_controller.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/audio_decoder_software_wrapper.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

namespace {

// Buffering parameters when load_type is kLoadTypeUrl.
const base::TimeDelta kLowBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(2000));
const base::TimeDelta kHighBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(6000));

// Buffering parameters when load_type is kLoadTypeMediaSource.
const base::TimeDelta kLowBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(0));
const base::TimeDelta kHighBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(300));

// Interval between two updates of the media time.
const base::TimeDelta kTimeUpdateInterval(
    base::TimeDelta::FromMilliseconds(250));

// Interval between two updates of the statistics is equal to:
// kTimeUpdateInterval * kStatisticsUpdatePeriod.
const int kStatisticsUpdatePeriod = 4;

}  // namespace

MediaPipelineImpl::MediaPipelineImpl()
    : cdm_(nullptr),
      audio_decoder_(nullptr),
      video_decoder_(nullptr),
      backend_initialized_(false),
      paused_(false),
      target_playback_rate_(1.0f),
      backend_started_(false),
      pending_time_update_task_(false),
      statistics_rolling_counter_(0),
      weak_factory_(this) {
  CMALOG(kLogControl) << __FUNCTION__;
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

MediaPipelineImpl::~MediaPipelineImpl() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  weak_factory_.InvalidateWeakPtrs();

  // Since av pipeline still need to access device components in their
  // destructor, it's important to delete them first.
  video_pipeline_.reset();
  audio_pipeline_.reset();
  audio_decoder_.reset();
  media_pipeline_backend_.reset();
  if (!client_.pipeline_backend_destroyed_cb.is_null())
    client_.pipeline_backend_destroyed_cb.Run();
}

void MediaPipelineImpl::Initialize(
    LoadType load_type,
    scoped_ptr<MediaPipelineBackend> media_pipeline_backend) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_decoder_.reset();
  media_pipeline_backend_.reset(media_pipeline_backend.release());
  if (!client_.pipeline_backend_created_cb.is_null())
    client_.pipeline_backend_created_cb.Run();

  if (load_type == kLoadTypeURL || load_type == kLoadTypeMediaSource) {
    base::TimeDelta low_threshold(kLowBufferThresholdURL);
    base::TimeDelta high_threshold(kHighBufferThresholdURL);
    if (load_type == kLoadTypeMediaSource) {
      low_threshold = kLowBufferThresholdMediaSource;
      high_threshold = kHighBufferThresholdMediaSource;
    }
    scoped_refptr<BufferingConfig> buffering_config(
        new BufferingConfig(low_threshold, high_threshold));
    buffering_controller_.reset(new BufferingController(
        buffering_config,
        base::Bind(&MediaPipelineImpl::OnBufferingNotification, weak_this_)));
  }
}

void MediaPipelineImpl::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client.error_cb.is_null());
  DCHECK(!client.time_update_cb.is_null());
  DCHECK(!client.buffering_state_cb.is_null());
  DCHECK(!client.pipeline_backend_created_cb.is_null());
  DCHECK(!client.pipeline_backend_destroyed_cb.is_null());
  client_ = client;
}

void MediaPipelineImpl::SetCdm(int cdm_id) {
  CMALOG(kLogControl) << __FUNCTION__ << " cdm_id=" << cdm_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(gunsch): SetCdm(int) is not implemented.
  // One possibility would be a GetCdmByIdCB that's passed in.
}

void MediaPipelineImpl::SetCdm(BrowserCdmCast* cdm) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  cdm_ = cdm;
  if (audio_pipeline_)
    audio_pipeline_->SetCdm(cdm);
  if (video_pipeline_)
    video_pipeline_->SetCdm(cdm);
}

void MediaPipelineImpl::InitializeAudio(
    const ::media::AudioDecoderConfig& config,
    const AvPipelineClient& client,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_decoder_);

  MediaPipelineBackend::AudioDecoder* backend_audio_decoder =
      media_pipeline_backend_->CreateAudioDecoder();
  if (!backend_audio_decoder) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  audio_decoder_.reset(new AudioDecoderSoftwareWrapper(backend_audio_decoder));
  audio_pipeline_.reset(new AudioPipelineImpl(audio_decoder_.get(), client));
  if (cdm_)
    audio_pipeline_->SetCdm(cdm_);
  audio_pipeline_->Initialize(config, std::move(frame_provider), status_cb);
}

void MediaPipelineImpl::InitializeVideo(
    const std::vector<::media::VideoDecoderConfig>& configs,
    const VideoPipelineClient& client,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!video_decoder_);

  video_decoder_ = media_pipeline_backend_->CreateVideoDecoder();
  if (!video_decoder_) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  video_pipeline_.reset(new VideoPipelineImpl(video_decoder_, client));
  if (cdm_)
    video_pipeline_->SetCdm(cdm_);
  video_pipeline_->Initialize(configs, std::move(frame_provider), status_cb);
}

void MediaPipelineImpl::StartPlayingFrom(base::TimeDelta time) {
  CMALOG(kLogControl) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_callbacks_);
  // When starting, we always enter the "playing" state (not paused).
  paused_ = false;

  // Lazy initialise
  if (!backend_initialized_) {
    backend_initialized_ = media_pipeline_backend_->Initialize();
    if (!backend_initialized_) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }

  // Start the backend.
  if (!media_pipeline_backend_->Start(time.InMicroseconds())) {
    OnError(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  // Enable time updates.
  backend_started_ = true;
  statistics_rolling_counter_ = 0;
  if (!pending_time_update_task_) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_));
  }

  // Setup the audio and video pipeline for the new timeline.
  if (audio_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("audio");
    if (!audio_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
  if (video_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("video");
    if (!video_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
}

void MediaPipelineImpl::Flush(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_callbacks_);

  backend_started_ = false;

  buffering_controller_->Reset();

  // Flush audio/video pipeline in three phases:
  // 1. Stop pushing data to backend. This guarentees media pipeline doesn't
  // push buffers to backend after stopping backend. (b/23751784)
  if (audio_pipeline_ && !audio_pipeline_->StartFlush()) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  if (video_pipeline_ && !video_pipeline_->StartFlush()) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  // 2. Stop the backend, so that the backend won't push their pending buffer,
  // which may be invalidated later, to hardware. (b/25342604)
  if (!media_pipeline_backend_->Stop()) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  // 3. Flush both the audio and video pipeline. This will flush the frame
  // provider and invalidate all the unreleased buffers.
  ::media::SerialRunner::Queue bound_fns;
  if (audio_pipeline_) {
    bound_fns.Push(base::Bind(&AudioPipelineImpl::Flush,
                              base::Unretained(audio_pipeline_.get())));
  }
  if (video_pipeline_) {
    bound_fns.Push(base::Bind(&VideoPipelineImpl::Flush,
                              base::Unretained(video_pipeline_.get())));
  }
  ::media::PipelineStatusCB transition_cb =
      base::Bind(&MediaPipelineImpl::OnFlushDone, weak_this_, status_cb);
  pending_flush_callbacks_ =
      ::media::SerialRunner::Run(bound_fns, transition_cb);
}

void MediaPipelineImpl::Stop() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);

  // Cancel pending flush callbacks since we are about to stop/shutdown
  // audio/video pipelines. This will ensure A/V Flush won't happen in
  // stopped state.
  pending_flush_callbacks_.reset();
  backend_started_ = false;

  // Stop both the audio and video pipeline.
  if (audio_pipeline_)
    audio_pipeline_->Stop();
  if (video_pipeline_)
    video_pipeline_->Stop();

  // Release hardware resources on Stop.
  audio_pipeline_ = nullptr;
  video_pipeline_ = nullptr;
  audio_decoder_.reset();
  media_pipeline_backend_.reset();
}

void MediaPipelineImpl::SetPlaybackRate(double rate) {
  CMALOG(kLogControl) << __FUNCTION__ << " rate=" << rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  target_playback_rate_ = rate;
  if (!backend_started_)
    return;
  if (buffering_controller_ && buffering_controller_->IsBuffering())
    return;

  if (rate != 0.0f) {
    media_pipeline_backend_->SetPlaybackRate(rate);
    if (paused_) {
      paused_ = false;
      media_pipeline_backend_->Resume();
    }
  } else if (!paused_) {
    paused_ = true;
    media_pipeline_backend_->Pause();
  }
}

void MediaPipelineImpl::SetVolume(float volume) {
  CMALOG(kLogControl) << __FUNCTION__ << " vol=" << volume;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_pipeline_)
    audio_pipeline_->SetVolume(volume);
}

void MediaPipelineImpl::OnFlushDone(const ::media::PipelineStatusCB& status_cb,
                                    ::media::PipelineStatus status) {
  // Clear pending buffers.
  if (audio_pipeline_)
    audio_pipeline_->BackendStopped();
  if (video_pipeline_)
    video_pipeline_->BackendStopped();

  pending_flush_callbacks_.reset();
  status_cb.Run(status);
}

void MediaPipelineImpl::OnBufferingNotification(bool is_buffering) {
  CMALOG(kLogControl) << __FUNCTION__ << " is_buffering=" << is_buffering;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffering_controller_);

  if (!client_.buffering_state_cb.is_null()) {
    ::media::BufferingState buffering_state =
        is_buffering ? ::media::BUFFERING_HAVE_NOTHING
                     : ::media::BUFFERING_HAVE_ENOUGH;
    client_.buffering_state_cb.Run(buffering_state);
  }

  if (!is_buffering) {
    DCHECK(!buffering_controller_->IsBuffering());
    // Once we finish buffering, we need to honour the desired playback rate
    // (rather than just resuming). This way, if playback was paused while
    // buffering, it will remain paused rather than incorrectly resuming.
    SetPlaybackRate(target_playback_rate_);
    return;
  }
  // Do not consume data in a rebuffering phase.
  if (!paused_) {
    paused_ = true;
    media_pipeline_backend_->Pause();
  }
}

void MediaPipelineImpl::UpdateMediaTime() {
  pending_time_update_task_ = false;
  if (!backend_started_)
    return;

  if (statistics_rolling_counter_ == 0) {
    if (audio_pipeline_)
      audio_pipeline_->UpdateStatistics();
    if (video_pipeline_)
      video_pipeline_->UpdateStatistics();
  }
  statistics_rolling_counter_ =
      (statistics_rolling_counter_ + 1) % kStatisticsUpdatePeriod;

  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(
      media_pipeline_backend_->GetCurrentPts());
  if (media_time == ::media::kNoTimestamp()) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
        kTimeUpdateInterval);
    return;
  }
  base::TimeTicks stc = base::TimeTicks::Now();

  base::TimeDelta max_rendering_time = media_time;
  if (buffering_controller_) {
    buffering_controller_->SetMediaTime(media_time);

    // Receiving the same time twice in a row means playback isn't moving,
    // so don't interpolate ahead.
    if (media_time != last_media_time_) {
      max_rendering_time = buffering_controller_->GetMaxRenderingTime();
      if (max_rendering_time == ::media::kNoTimestamp())
        max_rendering_time = media_time;

      // Cap interpolation time to avoid interpolating too far ahead.
      max_rendering_time =
          std::min(max_rendering_time, media_time + 2 * kTimeUpdateInterval);
    }
  }

  last_media_time_ = media_time;
  if (!client_.time_update_cb.is_null())
    client_.time_update_cb.Run(media_time, max_rendering_time, stc);

  pending_time_update_task_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
      kTimeUpdateInterval);
}

void MediaPipelineImpl::OnError(::media::PipelineStatus error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(error, ::media::PIPELINE_OK) << "PIPELINE_OK is not an error!";
  if (!client_.error_cb.is_null())
    client_.error_cb.Run(error);
}

}  // namespace media
}  // namespace chromecast
