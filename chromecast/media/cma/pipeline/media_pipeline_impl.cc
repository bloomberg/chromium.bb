// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/base/buffering_controller.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "media/base/buffers.h"

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
    : has_audio_(false),
      has_video_(false),
      target_playback_rate_(0.0),
      enable_time_update_(false),
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
}

void MediaPipelineImpl::Initialize(
    LoadType load_type,
    scoped_ptr<MediaPipelineDevice> media_pipeline_device) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_device_.reset(media_pipeline_device.release());
  clock_device_ = media_pipeline_device_->GetMediaClockDevice();

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

  audio_pipeline_.reset(new AudioPipelineImpl(
      media_pipeline_device_->GetAudioPipelineDevice()));

  video_pipeline_.reset(new VideoPipelineImpl(
      media_pipeline_device_->GetVideoPipelineDevice()));
}

void MediaPipelineImpl::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client.error_cb.is_null());
  DCHECK(!client.time_update_cb.is_null());
  DCHECK(!client.buffering_state_cb.is_null());
  client_ = client;
}

void MediaPipelineImpl::SetCdm(int cdm_id) {
  CMALOG(kLogControl) << __FUNCTION__ << " cdm_id=" << cdm_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(gunsch): SetCdm(int) is not implemented.
  // One possibility would be a GetCdmByIdCB that's passed in.
}

void MediaPipelineImpl::SetCdm(::media::BrowserCdm* media_keys) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_pipeline_->SetCdm(static_cast<BrowserCdmCast*>(media_keys));
  video_pipeline_->SetCdm(static_cast<BrowserCdmCast*>(media_keys));
}

AudioPipeline* MediaPipelineImpl::GetAudioPipeline() const {
  return audio_pipeline_.get();
}

VideoPipeline* MediaPipelineImpl::GetVideoPipeline() const {
  return video_pipeline_.get();
}

void MediaPipelineImpl::InitializeAudio(
    const ::media::AudioDecoderConfig& config,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!has_audio_);
  if (clock_device_->GetState() == MediaClockDevice::kStateUninitialized &&
      !clock_device_->SetState(MediaClockDevice::kStateIdle)) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  has_audio_ = true;
  audio_pipeline_->Initialize(config, frame_provider.Pass(), status_cb);
}

void MediaPipelineImpl::InitializeVideo(
    const ::media::VideoDecoderConfig& config,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!has_video_);
  if (clock_device_->GetState() == MediaClockDevice::kStateUninitialized &&
      !clock_device_->SetState(MediaClockDevice::kStateIdle)) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  has_video_ = true;
  video_pipeline_->Initialize(config, frame_provider.Pass(), status_cb);
}

void MediaPipelineImpl::StartPlayingFrom(base::TimeDelta time) {
  CMALOG(kLogControl) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio_ || has_video_);
  DCHECK(!pending_callbacks_);

  // Reset the start of the timeline.
  DCHECK_EQ(clock_device_->GetState(), MediaClockDevice::kStateIdle);
  clock_device_->ResetTimeline(time);

  // Start the clock. If the playback rate is 0, then the clock is started
  // but does not increase.
  if (!clock_device_->SetState(MediaClockDevice::kStateRunning)) {
    OnError(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  // Enable time updates.
  enable_time_update_ = true;
  statistics_rolling_counter_ = 0;
  if (!pending_time_update_task_) {
    pending_time_update_task_ = true;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_));
  }

  // Setup the audio and video pipeline for the new timeline.
  if (has_audio_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("audio");
    if (!audio_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
  if (has_video_) {
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
  DCHECK(has_audio_ || has_video_);
  DCHECK(!pending_callbacks_);

  // No need to update media time anymore.
  enable_time_update_ = false;

  buffering_controller_->Reset();

  // The clock should return to idle.
  if (!clock_device_->SetState(MediaClockDevice::kStateIdle)) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }

  // Flush both the audio and video pipeline.
  ::media::SerialRunner::Queue bound_fns;
  if (has_audio_) {
    bound_fns.Push(base::Bind(
        &AudioPipelineImpl::Flush,
        base::Unretained(audio_pipeline_.get())));
  }
  if (has_video_) {
    bound_fns.Push(base::Bind(
        &VideoPipelineImpl::Flush,
        base::Unretained(video_pipeline_.get())));
  }
  ::media::PipelineStatusCB transition_cb =
      base::Bind(&MediaPipelineImpl::StateTransition, weak_this_, status_cb);
  pending_callbacks_ =
      ::media::SerialRunner::Run(bound_fns, transition_cb);
}

void MediaPipelineImpl::Stop() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio_ || has_video_);
  DCHECK(!pending_callbacks_);

  // No need to update media time anymore.
  enable_time_update_ = false;

  // Release hardware resources on Stop.
  // Note: Stop can be called from any state.
  if (clock_device_->GetState() == MediaClockDevice::kStateRunning)
    clock_device_->SetState(MediaClockDevice::kStateIdle);
  if (clock_device_->GetState() == MediaClockDevice::kStateIdle)
    clock_device_->SetState(MediaClockDevice::kStateUninitialized);

  // Stop both the audio and video pipeline.
  if (has_audio_)
    audio_pipeline_->Stop();
  if (has_video_)
    video_pipeline_->Stop();
}

void MediaPipelineImpl::SetPlaybackRate(float rate) {
  CMALOG(kLogControl) << __FUNCTION__ << " rate=" << rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  target_playback_rate_ = rate;
  if (!buffering_controller_ || !buffering_controller_->IsBuffering())
    media_pipeline_device_->GetMediaClockDevice()->SetRate(rate);
}

AudioPipelineImpl* MediaPipelineImpl::GetAudioPipelineImpl() const {
  return audio_pipeline_.get();
}

VideoPipelineImpl* MediaPipelineImpl::GetVideoPipelineImpl() const {
  return video_pipeline_.get();
}

void MediaPipelineImpl::StateTransition(
    const ::media::PipelineStatusCB& status_cb,
    ::media::PipelineStatus status) {
  pending_callbacks_.reset();
  status_cb.Run(status);
}

void MediaPipelineImpl::OnBufferingNotification(bool is_buffering) {
  CMALOG(kLogControl) << __FUNCTION__ << " is_buffering=" << is_buffering;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffering_controller_);

  if (!client_.buffering_state_cb.is_null()) {
    ::media::BufferingState buffering_state = is_buffering ?
        ::media::BUFFERING_HAVE_NOTHING : ::media::BUFFERING_HAVE_ENOUGH;
    client_.buffering_state_cb.Run(buffering_state);
  }

  if (media_pipeline_device_->GetMediaClockDevice()->GetState() ==
      MediaClockDevice::kStateUninitialized) {
    return;
  }

  if (is_buffering) {
    // Do not consume data in a rebuffering phase.
    media_pipeline_device_->GetMediaClockDevice()->SetRate(0.0);
  } else {
    media_pipeline_device_->GetMediaClockDevice()->SetRate(
        target_playback_rate_);
  }
}

void MediaPipelineImpl::UpdateMediaTime() {
  pending_time_update_task_ = false;
  if (!enable_time_update_)
    return;

  if (statistics_rolling_counter_ == 0) {
    audio_pipeline_->UpdateStatistics();
    video_pipeline_->UpdateStatistics();
  }
  statistics_rolling_counter_ =
      (statistics_rolling_counter_ + 1) % kStatisticsUpdatePeriod;

  base::TimeDelta media_time(clock_device_->GetTime());
  if (media_time == ::media::kNoTimestamp()) {
    pending_time_update_task_ = true;
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
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
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
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
