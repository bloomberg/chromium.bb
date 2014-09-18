// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/renderer.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
class ServiceProvider;
}

namespace media {
class DemuxerStreamProvider;

// A media::Renderer that proxies to a mojo::MediaRenderer. That
// mojo::MediaRenderer proxies back to the MojoRendererImpl via the
// mojo::MediaRendererClient interface.
//
// MojoRendererImpl implements media::Renderer for use as either an audio
// or video renderer.
//
// TODO(tim): Only audio is currently supported. http://crbug.com/410451.
class MojoRendererImpl : public Renderer, public mojo::MediaRendererClient {
 public:
  // |task_runner| is the TaskRunner on which all methods are invoked.
  // |demuxer_stream_provider| provides encoded streams for decoding and
  //     rendering.
  // |audio_renderer_provider| is a ServiceProvider from a connected
  //     Application that is hosting a mojo::MediaRenderer.
  MojoRendererImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      DemuxerStreamProvider* demuxer_stream_provider,
      mojo::ServiceProvider* audio_renderer_provider);
  virtual ~MojoRendererImpl();

  // Renderer implementation.
  virtual void Initialize(const base::Closure& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const BufferingStateCB& buffering_state_cb) OVERRIDE;
  virtual void Flush(const base::Closure& flush_cb) OVERRIDE;
  virtual void StartPlayingFrom(base::TimeDelta time) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;
  virtual base::TimeDelta GetMediaTime() OVERRIDE;
  virtual bool HasAudio() OVERRIDE;
  virtual bool HasVideo() OVERRIDE;
  virtual void SetCdm(MediaKeys* cdm) OVERRIDE;

  // mojo::MediaRendererClient implementation.
  virtual void OnTimeUpdate(int64_t time_usec,
                            int64_t max_time_usec) MOJO_OVERRIDE;
  virtual void OnBufferingStateChange(mojo::BufferingState state) MOJO_OVERRIDE;
  virtual void OnEnded() MOJO_OVERRIDE;
  virtual void OnError() MOJO_OVERRIDE;

 private:
  // Called when |remote_audio_renderer_| has finished initializing.
  void OnInitialized();

  // Task runner used to execute pipeline tasks.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DemuxerStreamProvider* demuxer_stream_provider_;
  mojo::MediaRendererPtr remote_audio_renderer_;

  // Callbacks passed to Initialize() that we forward messages from
  // |remote_audio_renderer_| through.
  base::Closure init_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;

  base::WeakPtrFactory<MojoRendererImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MojoRendererImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
