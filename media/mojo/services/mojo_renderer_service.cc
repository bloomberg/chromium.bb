// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/media_log.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/mojo/services/mojo_demuxer_stream_adapter.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"

namespace media {

class MojoRendererApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::MediaRenderer> {
 public:
  // mojo::ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  virtual void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::MediaRenderer> request) OVERRIDE {
    mojo::BindToRequest(new MojoRendererService(connection), &request);
  }
};

MojoRendererService::MojoRendererService(
    mojo::ApplicationConnection* connection)
    : hardware_config_(AudioParameters(), AudioParameters()),
      weak_factory_(this),
      weak_this_(weak_factory_.GetWeakPtr()) {
  scoped_refptr<base::SingleThreadTaskRunner> runner(
      base::MessageLoop::current()->task_runner());
  scoped_refptr<MediaLog> media_log(new MediaLog());
  audio_renderer_.reset(new AudioRendererImpl(
      runner,
      // TODO(tim): We should use |connection| passed to MojoRendererService
      // to connect to a MojoAudioRendererSink implementation that we would
      // wrap in an AudioRendererSink and pass in here.
      new NullAudioSink(runner),
      // TODO(tim): Figure out how to select decoders.
      ScopedVector<AudioDecoder>(),
      // TODO(tim): Not needed for now?
      SetDecryptorReadyCB(),
      hardware_config_,
      media_log));
}

MojoRendererService::~MojoRendererService() {
}

void MojoRendererService::Initialize(mojo::DemuxerStreamPtr stream,
                                     const mojo::Callback<void()>& callback) {
  DCHECK(client());
  stream_.reset(new MojoDemuxerStreamAdapter(
      stream.Pass(),
      base::Bind(&MojoRendererService::OnStreamReady, weak_this_)));
  init_cb_ = callback;
}

void MojoRendererService::Flush(const mojo::Callback<void()>& callback) {
  NOTIMPLEMENTED();
}

void MojoRendererService::StartPlayingFrom(int64_t time_delta_usec) {
  NOTIMPLEMENTED();
}

void MojoRendererService::SetPlaybackRate(float playback_rate) {
  NOTIMPLEMENTED();
}

void MojoRendererService::SetVolume(float volume) {
  NOTIMPLEMENTED();
}

void MojoRendererService::OnStreamReady() {
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
  if (status != PIPELINE_OK) {
    audio_renderer_.reset();
    client()->OnError();
  }
  init_cb_.Run();
}

void MojoRendererService::OnUpdateStatistics(const PipelineStatistics& stats) {
  NOTIMPLEMENTED();
}

void MojoRendererService::OnAudioTimeUpdate(base::TimeDelta time,
                                            base::TimeDelta max_time) {
  client()->OnTimeUpdate(time.InMicroseconds(), max_time.InMicroseconds());
}

void MojoRendererService::OnBufferingStateChanged(
    media::BufferingState new_buffering_state) {
  client()->OnBufferingStateChange(
      static_cast<mojo::BufferingState>(new_buffering_state));
}

void MojoRendererService::OnAudioRendererEnded() {
  client()->OnEnded();
}

void MojoRendererService::OnError(PipelineStatus error) {
  client()->OnError();
}

}  // namespace media

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new media::MojoRendererApplication);
  return runner.Run(shell_handle);
}
