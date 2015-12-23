// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/renderer.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class DemuxerStreamProvider;

// A media::Renderer that proxies to a interfaces::Renderer. That
// interfaces::Renderer proxies back to the MojoRendererImpl via the
// interfaces::RendererClient interface.
//
// MojoRendererImpl implements media::Renderer for use as either an audio
// or video renderer.
class MojoRendererImpl : public Renderer, public interfaces::RendererClient {
 public:
  // |task_runner| is the TaskRunner on which all methods are invoked.
  MojoRendererImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      interfaces::RendererPtr remote_renderer);
  ~MojoRendererImpl() override;

  // Renderer implementation.
  void Initialize(DemuxerStreamProvider* demuxer_stream_provider,
                  const PipelineStatusCB& init_cb,
                  const StatisticsCB& statistics_cb,
                  const BufferingStateCB& buffering_state_cb,
                  const base::Closure& ended_cb,
                  const PipelineStatusCB& error_cb,
                  const base::Closure& waiting_for_decryption_key_cb) override;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) override;
  void Flush(const base::Closure& flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  bool HasAudio() override;
  bool HasVideo() override;

  // interfaces::RendererClient implementation.
  void OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) override;
  void OnBufferingStateChange(interfaces::BufferingState state) override;
  void OnEnded() override;
  void OnError() override;

 private:
  // Called when |remote_renderer_| has finished initializing.
  void OnInitialized(bool success);

  // Task runner used to execute pipeline tasks.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DemuxerStreamProvider* demuxer_stream_provider_;
  interfaces::RendererPtr remote_renderer_;
  scoped_ptr<mojo::Binding<RendererClient>> binding_;

  // Callbacks passed to Initialize() that we forward messages from
  // |remote_renderer_| through.
  PipelineStatusCB init_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;

  // Lock used to serialize access for the following data members.
  mutable base::Lock lock_;

  base::TimeDelta time_;
  // TODO(xhwang): It seems we don't need |max_time_| now. Drop it!
  base::TimeDelta max_time_;

  base::WeakPtrFactory<MojoRendererImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
