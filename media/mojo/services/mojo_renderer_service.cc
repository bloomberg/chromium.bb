// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_vector.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/media_log.h"
#include "media/base/video_renderer.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/renderer_impl.h"
#include "media/filters/video_renderer_impl.h"
#include "media/mojo/services/demuxer_stream_provider_shim.h"
#include "media/mojo/services/mojo_demuxer_stream_adapter.h"
#include "media/mojo/services/renderer_config.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"

namespace media {

// Time interval to update media time.
const int kTimeUpdateIntervalMs = 50;

static void LogMediaSourceError(const scoped_refptr<MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

static void PaintNothing(const scoped_refptr<VideoFrame>& frame) {
}

class MojoRendererApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::MediaRenderer> {
 public:
  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::MediaRenderer> request) override {
    mojo::BindToRequest(new MojoRendererService(connection), &request);
  }
};

static void MojoTrampoline(const mojo::Closure& closure) {
  closure.Run();
}

MojoRendererService::MojoRendererService(
    mojo::ApplicationConnection* connection)
    : state_(STATE_UNINITIALIZED),
      last_media_time_usec_(0),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()) {
  DVLOG(1) << __FUNCTION__;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MessageLoop::current()->task_runner());
  scoped_refptr<MediaLog> media_log(new MediaLog());
  RendererConfig* renderer_config = RendererConfig::Get();
  audio_renderer_sink_ = renderer_config->GetAudioRendererSink();

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      task_runner,
      audio_renderer_sink_.get(),
      renderer_config->GetAudioDecoders(
                           task_runner,
                           base::Bind(&LogMediaSourceError, media_log)).Pass(),
      SetDecryptorReadyCB(),
      renderer_config->GetAudioHardwareConfig(),
      media_log));

  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      task_runner,
      renderer_config->GetVideoDecoders(
                           task_runner,
                           base::Bind(&LogMediaSourceError, media_log)).Pass(),
      SetDecryptorReadyCB(),
      base::Bind(&PaintNothing),
      true,
      media_log));

  // Create renderer.
  renderer_.reset(new RendererImpl(
      task_runner, audio_renderer.Pass(), video_renderer.Pass()));
}

MojoRendererService::~MojoRendererService() {
}

void MojoRendererService::Initialize(mojo::DemuxerStreamPtr audio,
                                     mojo::DemuxerStreamPtr video,
                                     const mojo::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(client());

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
  time_update_timer_.Reset();
  renderer_->Flush(base::Bind(&MojoTrampoline, callback));
}

void MojoRendererService::StartPlayingFrom(int64_t time_delta_usec) {
  DVLOG(2) << __FUNCTION__ << ": " << time_delta_usec;
  renderer_->StartPlayingFrom(
      base::TimeDelta::FromMicroseconds(time_delta_usec));
  SchedulePeriodicMediaTimeUpdates();
}

void MojoRendererService::SetPlaybackRate(float playback_rate) {
  DVLOG(2) << __FUNCTION__ << ": " << playback_rate;
  DCHECK_EQ(state_, STATE_PLAYING);
  renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererService::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

void MojoRendererService::OnStreamReady(const mojo::Closure& callback) {
  DCHECK_EQ(state_, STATE_INITIALIZING);

  renderer_->Initialize(
      stream_provider_.get(),
      base::Bind(
          &MojoRendererService::OnRendererInitializeDone, weak_this_, callback),
      base::Bind(&MojoRendererService::OnUpdateStatistics, weak_this_),
      base::Bind(&MojoRendererService::OnRendererEnded, weak_this_),
      base::Bind(&MojoRendererService::OnError, weak_this_),
      base::Bind(&MojoRendererService::OnBufferingStateChanged, weak_this_));
}

void MojoRendererService::OnRendererInitializeDone(
    const mojo::Closure& callback) {
  DVLOG(1) << __FUNCTION__;

  if (state_ == STATE_ERROR) {
    renderer_.reset();
  } else {
    DCHECK_EQ(state_, STATE_INITIALIZING);
    state_ = STATE_PLAYING;
  }

  callback.Run();
}

void MojoRendererService::OnUpdateStatistics(const PipelineStatistics& stats) {
}

void MojoRendererService::UpdateMediaTime(bool force) {
  const uint64_t media_time = renderer_->GetMediaTime().InMicroseconds();
  if (!force && media_time == last_media_time_usec_)
    return;

  client()->OnTimeUpdate(media_time, media_time);
  last_media_time_usec_ = media_time;
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  UpdateMediaTime(true);
  time_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTimeUpdateIntervalMs),
      base::Bind(&MojoRendererService::UpdateMediaTime, weak_this_, false));
}

void MojoRendererService::OnBufferingStateChanged(
    BufferingState new_buffering_state) {
  DVLOG(2) << __FUNCTION__ << "(" << new_buffering_state << ") ";
  client()->OnBufferingStateChange(
      static_cast<mojo::BufferingState>(new_buffering_state));
}

void MojoRendererService::OnRendererEnded() {
  DVLOG(1) << __FUNCTION__;
  client()->OnEnded();
  time_update_timer_.Reset();
}

void MojoRendererService::OnError(PipelineStatus error) {
  DVLOG(1) << __FUNCTION__;
  state_ = STATE_ERROR;
  client()->OnError();
}

}  // namespace media

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new media::MojoRendererApplication);
  return runner.Run(shell_handle);
}
