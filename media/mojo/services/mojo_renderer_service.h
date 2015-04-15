// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_impl.h"

namespace mojo {
class ApplicationConnection;
}

namespace media {

class AudioRendererSink;
class DemuxerStreamProviderShim;
class Renderer;

// A mojo::MediaRenderer implementation that uses media::AudioRenderer to
// decode and render audio to a sink obtained from the ApplicationConnection.
class MEDIA_EXPORT MojoRendererService
    : NON_EXPORTED_BASE(public mojo::InterfaceImpl<mojo::MediaRenderer>) {
 public:
  MojoRendererService();
  ~MojoRendererService() override;

  // mojo::MediaRenderer implementation.
  void Initialize(mojo::MediaRendererClientPtr client,
                  mojo::DemuxerStreamPtr audio,
                  mojo::DemuxerStreamPtr video,
                  const mojo::Closure& callback) override;
  void Flush(const mojo::Closure& callback) override;
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

  // Called when the DemuxerStreamProviderShim is ready to go (has a config,
  // pipe handle, etc) and can be handed off to a renderer for use.
  void OnStreamReady(const mojo::Closure& callback);

  // Called when |audio_renderer_| initialization has completed.
  void OnRendererInitializeDone(const mojo::Closure& callback,
                                PipelineStatus status);

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats);

  // Periodically polls the media time from the renderer and notifies the client
  // if the media time has changed since the last update.  If |force| is true,
  // the client is notified even if the time is unchanged.
  void UpdateMediaTime(bool force);
  void CancelPeriodicMediaTimeUpdates();
  void SchedulePeriodicMediaTimeUpdates();

  // Callback executed by audio renderer when buffering state changes.
  // TODO(tim): Need old and new.
  void OnBufferingStateChanged(BufferingState new_buffering_state);

  // Callback executed when a renderer has ended.
  void OnRendererEnded();

  // Callback executed when a runtime error happens.
  void OnError(PipelineStatus error);

  // Callback executed once Flush() completes.
  void OnFlushCompleted(const mojo::Closure& callback);

  State state_;

  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_ptr<Renderer> renderer_;
  scoped_ptr<DemuxerStreamProviderShim> stream_provider_;

  base::RepeatingTimer<MojoRendererService> time_update_timer_;
  uint64_t last_media_time_usec_;

  mojo::MediaRendererClientPtr client_;

  base::WeakPtr<MojoRendererService> weak_this_;
  base::WeakPtrFactory<MojoRendererService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_SERVICE_H_
