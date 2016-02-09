// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
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
// This class can be created on any thread, where the |remote_renderer| is
// connected and passed in the constructor. Then Initialize() will be called on
// the |task_runner| and starting from that point this class is bound to the
// |task_runner|*. That means all Renderer and RendererClient methods will be
// called/dispached on the |task_runner|. The only exception is GetMediaTime(),
// which can be called on any thread.
//
// * Threading details:
// mojo::GetProxy() doesn't bind an InterfacePtr to a thread. Then when
// InterfacePtr::operator->() or InterfacePtr::get() is called for the first
// time, e.g. to call remote_renderer->Initialize(), the InterfacePtr is bound
// the thread where the call is made.

class MojoRendererImpl : public Renderer, public interfaces::RendererClient {
 public:
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

 private:
  // interfaces::RendererClient implementation, dispatched on the
  // |task_runner_|.
  void OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) override;
  void OnBufferingStateChange(interfaces::BufferingState state) override;
  void OnEnded() override;
  void OnError() override;

  // Callback for connection error on |remote_renderer_|.
  void OnConnectionError();

  // Called when |remote_renderer_| has finished initializing.
  void OnInitialized(bool success);

  // |task_runner| on which all methods are invoked, except for GetMediaTime(),
  // which can be called on any thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Provider of audio/video DemuxerStreams. Must be valid throughout the
  // lifetime of |this|.
  DemuxerStreamProvider* demuxer_stream_provider_;

  // Remote Renderer, bound to |task_runner_|.
  interfaces::RendererPtr remote_renderer_;

  // Binding for RendererClient, bound to the |task_runner_|.
  mojo::Binding<RendererClient> binding_;

  // Callbacks passed to Initialize() that we forward messages from
  // |remote_renderer_| through.
  PipelineStatusCB init_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;

  // Lock used to serialize access for |time_|.
  mutable base::Lock lock_;
  base::TimeDelta time_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_IMPL_H_
