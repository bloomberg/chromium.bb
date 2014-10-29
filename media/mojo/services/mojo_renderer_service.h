// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "mojo/public/cpp/bindings/interface_impl.h"

namespace mojo {
class ApplicationConnection;
}

namespace media {

class AudioRenderer;
class AudioRendererSink;
class MojoDemuxerStreamAdapter;
class TimeSource;

// A mojo::MediaRenderer implementation that uses media::AudioRenderer to
// decode and render audio to a sink obtained from the ApplicationConnection.
class MojoRendererService : public mojo::InterfaceImpl<mojo::MediaRenderer> {
 public:
  // |connection| is a pointer to the connection back to our embedder. The
  // embedder should have configured it (via ConfigureOutgoingConnection) to
  // allow |this| to connect to a sink that will receive decoded data ready
  // for playback.
  explicit MojoRendererService(mojo::ApplicationConnection* connection);
  ~MojoRendererService() override;

  // mojo::MediaRenderer implementation.
  void Initialize(mojo::DemuxerStreamPtr stream,
                  const mojo::Callback<void()>& callback) override;
  void Flush(const mojo::Callback<void()>& callback) override;
  void StartPlayingFrom(int64_t time_delta_usec) override;
  void SetPlaybackRate(float playback_rate) override;
  void SetVolume(float volume) override;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  // Called when the MojoDemuxerStreamAdapter is ready to go (has a config,
  // pipe handle, etc) and can be handed off to a renderer for use.
  void OnStreamReady();

  // Called when |audio_renderer_| initialization has completed.
  void OnAudioRendererInitializeDone(PipelineStatus status);

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats);

  void UpdateMediaTime();
  void SchedulePeriodicMediaTimeUpdates();

  // Callback executed by audio renderer when buffering state changes.
  // TODO(tim): Need old and new.
  void OnBufferingStateChanged(BufferingState new_buffering_state);

  // Callback executed when a renderer has ended.
  void OnAudioRendererEnded();

  // Callback executed when a runtime error happens.
  void OnError(PipelineStatus error);

  bool WaitingForEnoughData() const;
  void StartPlayback();
  void PausePlayback();

  State state_;

  scoped_ptr<MojoDemuxerStreamAdapter> stream_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_ptr<AudioRenderer> audio_renderer_;

  TimeSource* time_source_;
  bool time_ticking_;

  BufferingState buffering_state_;

  mojo::Callback<void()> init_cb_;

  bool ended_;

  base::RepeatingTimer<MojoRendererService> time_update_timer_;

  base::WeakPtrFactory<MojoRendererService> weak_factory_;
  base::WeakPtr<MojoRendererService> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
