// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/services/demuxer_stream_provider_shim.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"

namespace media {

// Time interval to update media time.
const int kTimeUpdateIntervalMs = 50;

MojoRendererService::MojoRendererService(
    base::WeakPtr<CdmContextProvider> cdm_context_provider,
    RendererFactory* renderer_factory,
    const scoped_refptr<MediaLog>& media_log,
    mojo::InterfaceRequest<interfaces::Renderer> request)
    : binding_(this, request.Pass()),
      cdm_context_provider_(cdm_context_provider),
      state_(STATE_UNINITIALIZED),
      last_media_time_usec_(0),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  DVLOG(1) << __FUNCTION__;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MessageLoop::current()->task_runner());
  MojoMediaClient* mojo_media_client = MojoMediaClient::Get();
  audio_renderer_sink_ = mojo_media_client->CreateAudioRendererSink();
  video_renderer_sink_ =
      mojo_media_client->CreateVideoRendererSink(task_runner);

  // Create renderer.
  if (renderer_factory) {
    renderer_ = renderer_factory->CreateRenderer(task_runner, task_runner,
                                                 audio_renderer_sink_.get(),
                                                 video_renderer_sink_.get());
  } else {
    DCHECK(mojo_media_client->GetAudioHardwareConfig());
    scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
        task_runner, audio_renderer_sink_.get(),
        mojo_media_client->CreateAudioDecoders(task_runner, media_log).Pass(),
        *mojo_media_client->GetAudioHardwareConfig(), media_log));
    scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
        task_runner, task_runner, video_renderer_sink_.get(),
        mojo_media_client->CreateVideoDecoders(task_runner, media_log).Pass(),
        true, nullptr, media_log));
    renderer_.reset(new RendererImpl(task_runner, audio_renderer.Pass(),
                                     video_renderer.Pass()));
  }
}

MojoRendererService::~MojoRendererService() {
}

void MojoRendererService::Initialize(
    interfaces::RendererClientPtr client,
    interfaces::DemuxerStreamPtr audio,
    interfaces::DemuxerStreamPtr video,
    const mojo::Callback<void(bool)>& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  client_ = client.Pass();
  state_ = STATE_INITIALIZING;
  stream_provider_.reset(new DemuxerStreamProviderShim(
      audio.Pass(),
      video.Pass(),
      base::Bind(&MojoRendererService::OnStreamReady, weak_this_, callback)));
}

void MojoRendererService::Flush(const mojo::Closure& callback) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_PLAYING);

  state_ = STATE_FLUSHING;
  CancelPeriodicMediaTimeUpdates();
  renderer_->Flush(
      base::Bind(&MojoRendererService::OnFlushCompleted, weak_this_, callback));
}

void MojoRendererService::StartPlayingFrom(int64_t time_delta_usec) {
  DVLOG(2) << __FUNCTION__ << ": " << time_delta_usec;
  renderer_->StartPlayingFrom(
      base::TimeDelta::FromMicroseconds(time_delta_usec));
  SchedulePeriodicMediaTimeUpdates();
}

void MojoRendererService::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__ << ": " << playback_rate;
  DCHECK_EQ(state_, STATE_PLAYING);
  renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererService::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

void MojoRendererService::SetCdm(int32_t cdm_id,
                                 const mojo::Callback<void(bool)>& callback) {
  if (!cdm_context_provider_) {
    LOG(ERROR) << "CDM context provider not available.";
    callback.Run(false);
    return;
  }

  CdmContext* cdm_context = cdm_context_provider_->GetCdmContext(cdm_id);
  if (!cdm_context) {
    LOG(ERROR) << "CDM context not found: " << cdm_id;
    callback.Run(false);
    return;
  }

  renderer_->SetCdm(cdm_context, base::Bind(&MojoRendererService::OnCdmAttached,
                                            weak_this_, callback));
}

void MojoRendererService::OnStreamReady(
    const mojo::Callback<void(bool)>& callback) {
  DCHECK_EQ(state_, STATE_INITIALIZING);

  renderer_->Initialize(
      stream_provider_.get(),
      base::Bind(
          &MojoRendererService::OnRendererInitializeDone, weak_this_, callback),
      base::Bind(&MojoRendererService::OnUpdateStatistics, weak_this_),
      base::Bind(&MojoRendererService::OnBufferingStateChanged, weak_this_),
      base::Bind(&MojoRendererService::OnRendererEnded, weak_this_),
      base::Bind(&MojoRendererService::OnError, weak_this_),
      base::Bind(base::DoNothing));
}

void MojoRendererService::OnRendererInitializeDone(
    const mojo::Callback<void(bool)>& callback,
    PipelineStatus status) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_INITIALIZING);

  if (status != PIPELINE_OK) {
    state_ = STATE_ERROR;
    callback.Run(false);
    return;
  }

  state_ = STATE_PLAYING;
  callback.Run(true);
}

void MojoRendererService::OnUpdateStatistics(const PipelineStatistics& stats) {
}

void MojoRendererService::UpdateMediaTime(bool force) {
  const uint64_t media_time = renderer_->GetMediaTime().InMicroseconds();
  if (!force && media_time == last_media_time_usec_)
    return;

  client_->OnTimeUpdate(media_time, media_time);
  last_media_time_usec_ = media_time;
}

void MojoRendererService::CancelPeriodicMediaTimeUpdates() {
  DVLOG(2) << __FUNCTION__;
  UpdateMediaTime(false);
  time_update_timer_.Stop();
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  DVLOG(2) << __FUNCTION__;
  UpdateMediaTime(true);
  time_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTimeUpdateIntervalMs),
      base::Bind(&MojoRendererService::UpdateMediaTime, weak_this_, false));
}

void MojoRendererService::OnBufferingStateChanged(
    BufferingState new_buffering_state) {
  DVLOG(2) << __FUNCTION__ << "(" << new_buffering_state << ")";
  client_->OnBufferingStateChange(
      static_cast<interfaces::BufferingState>(new_buffering_state));
}

void MojoRendererService::OnRendererEnded() {
  DVLOG(1) << __FUNCTION__;
  CancelPeriodicMediaTimeUpdates();
  client_->OnEnded();
}

void MojoRendererService::OnError(PipelineStatus error) {
  DVLOG(1) << __FUNCTION__ << "(" << error << ")";
  state_ = STATE_ERROR;
  client_->OnError();
}

void MojoRendererService::OnFlushCompleted(const mojo::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_FLUSHING);
  state_ = STATE_PLAYING;
  callback.Run();
}

void MojoRendererService::OnCdmAttached(
    const mojo::Callback<void(bool)>& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";
  callback.Run(success);
}

}  // namespace media
