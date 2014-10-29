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
#include "media/filters/audio_renderer_impl.h"
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

static base::TimeDelta TimeUpdateInterval() {
  return base::TimeDelta::FromMilliseconds(kTimeUpdateIntervalMs);
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

// TODO(xhwang): This class looks insanely similar to RendererImpl. We should
// really host a Renderer in this class instead of a AudioRenderer.

MojoRendererService::MojoRendererService(
    mojo::ApplicationConnection* connection)
    : state_(STATE_UNINITIALIZED),
      time_source_(NULL),
      time_ticking_(false),
      ended_(false),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()) {
  DVLOG(1) << __FUNCTION__;

  scoped_refptr<base::SingleThreadTaskRunner> runner(
      base::MessageLoop::current()->task_runner());
  scoped_refptr<MediaLog> media_log(new MediaLog());
  RendererConfig* renderer_config = RendererConfig::Get();
  audio_renderer_sink_ = renderer_config->GetAudioRendererSink();

  audio_renderer_.reset(new AudioRendererImpl(
      runner,
      audio_renderer_sink_.get(),
      renderer_config->GetAudioDecoders(
                           runner, base::Bind(&LogMediaSourceError, media_log))
          .Pass(),
      SetDecryptorReadyCB(),
      renderer_config->GetAudioHardwareConfig(),
      media_log));
}

MojoRendererService::~MojoRendererService() {
}

void MojoRendererService::Initialize(mojo::DemuxerStreamPtr stream,
                                     const mojo::Callback<void()>& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_UNINITIALIZED) << state_;
  DCHECK(client());

  init_cb_ = callback;
  state_ = STATE_INITIALIZING;
  stream_.reset(new MojoDemuxerStreamAdapter(
      stream.Pass(),
      base::Bind(&MojoRendererService::OnStreamReady, weak_this_)));
}

void MojoRendererService::Flush(const mojo::Callback<void()>& callback) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_PLAYING) << state_;

  state_ = STATE_FLUSHING;
  if (time_ticking_)
    PausePlayback();

  // TODO(xhwang): This is not completed. Finish the flushing path.
  NOTIMPLEMENTED();
}

void MojoRendererService::StartPlayingFrom(int64_t time_delta_usec) {
  DVLOG(2) << __FUNCTION__ << ": " << time_delta_usec;
  base::TimeDelta time = base::TimeDelta::FromMicroseconds(time_delta_usec);
  time_source_->SetMediaTime(time);
  audio_renderer_->StartPlaying();
}

void MojoRendererService::SetPlaybackRate(float playback_rate) {
  DVLOG(2) << __FUNCTION__ << ": " << playback_rate;

  // Playback rate changes are only carried out while playing.
  if (state_ != STATE_PLAYING)
    return;

  time_source_->SetPlaybackRate(playback_rate);
}

void MojoRendererService::SetVolume(float volume) {
  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

void MojoRendererService::OnStreamReady() {
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  audio_renderer_->Initialize(
      stream_.get(),
      base::Bind(&MojoRendererService::OnAudioRendererInitializeDone,
                 weak_this_),
      base::Bind(&MojoRendererService::OnUpdateStatistics, weak_this_),
      base::Bind(&MojoRendererService::OnBufferingStateChanged, weak_this_),
      base::Bind(&MojoRendererService::OnAudioRendererEnded, weak_this_),
      base::Bind(&MojoRendererService::OnError, weak_this_));
}

void MojoRendererService::OnAudioRendererInitializeDone(PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << ": " << status;
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;

  if (status != PIPELINE_OK) {
    state_ = STATE_ERROR;
    audio_renderer_.reset();
    client()->OnError();
    init_cb_.Run();
    init_cb_.reset();
    return;
  }

  time_source_ = audio_renderer_->GetTimeSource();

  state_ = STATE_PLAYING;
  init_cb_.Run();
  init_cb_.reset();
}

void MojoRendererService::OnUpdateStatistics(const PipelineStatistics& stats) {
  NOTIMPLEMENTED();
}

void MojoRendererService::UpdateMediaTime() {
  uint64_t media_time = time_source_->CurrentMediaTime().InMicroseconds();
  client()->OnTimeUpdate(media_time, media_time);
}

void MojoRendererService::SchedulePeriodicMediaTimeUpdates() {
  // Update media time immediately.
  UpdateMediaTime();

  // Then setup periodic time update.
  time_update_timer_.Start(
      FROM_HERE,
      TimeUpdateInterval(),
      base::Bind(&MojoRendererService::UpdateMediaTime, weak_this_));
}

void MojoRendererService::OnBufferingStateChanged(
    media::BufferingState new_buffering_state) {
  DVLOG(2) << __FUNCTION__ << "(" << buffering_state_ << ", "
           << new_buffering_state << ") ";
  bool was_waiting_for_enough_data = WaitingForEnoughData();

  buffering_state_ = new_buffering_state;

  // Renderer underflowed.
  if (!was_waiting_for_enough_data && WaitingForEnoughData()) {
    PausePlayback();
    // TODO(xhwang): Notify client of underflow condition.
    return;
  }

  // Renderer prerolled.
  if (was_waiting_for_enough_data && !WaitingForEnoughData()) {
    StartPlayback();
    client()->OnBufferingStateChange(
        static_cast<mojo::BufferingState>(new_buffering_state));
    return;
  }
}

void MojoRendererService::OnAudioRendererEnded() {
  DVLOG(1) << __FUNCTION__;

  if (state_ != STATE_PLAYING)
    return;

  DCHECK(!ended_);
  ended_ = true;

  if (time_ticking_)
    PausePlayback();

  client()->OnEnded();
}

void MojoRendererService::OnError(PipelineStatus error) {
  client()->OnError();
}

bool MojoRendererService::WaitingForEnoughData() const {
  DCHECK(audio_renderer_);

  return state_ == STATE_PLAYING && buffering_state_ != BUFFERING_HAVE_ENOUGH;
}

void MojoRendererService::StartPlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, STATE_PLAYING);
  DCHECK(!time_ticking_);
  DCHECK(!WaitingForEnoughData());

  time_ticking_ = true;
  time_source_->StartTicking();

  SchedulePeriodicMediaTimeUpdates();
}

void MojoRendererService::PausePlayback() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(time_ticking_);
  switch (state_) {
    case STATE_PLAYING:
      DCHECK(ended_ || WaitingForEnoughData())
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

  // Cancel repeating time update timer and update the current media time.
  time_update_timer_.Stop();
  UpdateMediaTime();
}

}  // namespace media

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new media::MojoRendererApplication);
  return runner.Run(shell_handle);
}
