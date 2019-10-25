// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/default_tick_clock.h"
#include "base/unguessable_token.h"
#include "media/base/demuxer_stream.h"
#include "media/base/renderer.h"
#include "media/base/time_delta_interpolator.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaResource;
class MojoDemuxerStreamImpl;
class VideoOverlayFactory;
class VideoRendererSink;

// A media::Renderer that proxies to a mojom::Renderer. That
// mojom::Renderer proxies back to the MojoRenderer via the
// mojom::RendererClient interface.
//
// This class can be created on any thread, where the |remote_renderer| is
// connected and passed in the constructor. Then Initialize() will be called on
// the |task_runner| and starting from that point this class is bound to the
// |task_runner|*. That means all Renderer and RendererClient methods will be
// called/dispached on the |task_runner|. The only exception is GetMediaTime(),
// which can be called on any thread.
class MojoRenderer : public Renderer, public mojom::RendererClient {
 public:
  MojoRenderer(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
               std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
               VideoRendererSink* video_renderer_sink,
               mojom::RendererPtr remote_renderer);
  ~MojoRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  media::RendererClient* client,
                  const PipelineStatusCB& init_cb) override;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

 private:
  // mojom::RendererClient implementation, dispatched on the
  // |task_runner_|.
  void OnTimeUpdate(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnEnded() override;
  void OnError() override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnWaiting(WaitingReason reason) override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;

  // Binds |remote_renderer_| to the mojo message pipe. Can be called multiple
  // times. If an error occurs during connection, OnConnectionError will be
  // called asynchronously.
  void BindRemoteRendererIfNeeded();

  // Initialize the remote renderer when |media_resource| is of type
  // MediaResource::Type::STREAM.
  void InitializeRendererFromStreams(media::RendererClient* client);

  // Initialize the remote renderer when |media_resource| is of type
  // MediaResource::Type::URL.
  void InitializeRendererFromUrl(media::RendererClient* client);

  // Callback for connection error on |remote_renderer_|.
  void OnConnectionError();

  // Callback for connection error on any of |streams_|. The |stream| parameter
  // indicates which stream the error happened on.
  void OnDemuxerStreamConnectionError(MojoDemuxerStreamImpl* stream);

  // Callbacks for |remote_renderer_| methods.
  void OnInitialized(media::RendererClient* client, bool success);
  void OnFlushed();
  void OnCdmAttached(bool success);

  void CancelPendingCallbacks();

  // |task_runner| on which all methods are invoked, except for GetMediaTime(),
  // which can be called on any thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Overlay factory used to create overlays for video frames rendered
  // by the remote renderer.
  std::unique_ptr<VideoOverlayFactory> video_overlay_factory_;

  // Video frame overlays are rendered onto this sink.
  // Rendering of a new overlay is only needed when video natural size changes.
  VideoRendererSink* video_renderer_sink_ = nullptr;

  // Provider of audio/video DemuxerStreams. Must be valid throughout the
  // lifetime of |this|.
  MediaResource* media_resource_ = nullptr;

  // Client of |this| renderer passed in Initialize.
  media::RendererClient* client_ = nullptr;

  // Mojo demuxer streams.
  // Owned by MojoRenderer instead of remote mojom::Renderer
  // becuase these demuxer streams need to be destroyed as soon as |this| is
  // destroyed. The local demuxer streams returned by MediaResource cannot be
  // used after |this| is destroyed.
  // TODO(alokp): Add tests for MojoDemuxerStreamImpl.
  std::vector<std::unique_ptr<MojoDemuxerStreamImpl>> streams_;

  // This class is constructed on one thread and used exclusively on another
  // thread. This member is used to safely pass the RendererPtr from one thread
  // to another. It is set in the constructor and is consumed in Initialize().
  mojom::RendererPtrInfo remote_renderer_info_;

  // Remote Renderer, bound to |task_runner_| during Initialize().
  mojom::RendererPtr remote_renderer_;

  // Binding for RendererClient, bound to the |task_runner_|.
  mojo::AssociatedBinding<RendererClient> client_binding_;

  bool encountered_error_ = false;

  PipelineStatusCB init_cb_;
  base::OnceClosure flush_cb_;
  CdmAttachedCB cdm_attached_cb_;

  // Lock used to serialize access for |time_interpolator_|.
  mutable base::Lock lock_;
  media::TimeDeltaInterpolator media_time_interpolator_;

  base::Optional<PipelineStatistics> pending_stats_;

  DISALLOW_COPY_AND_ASSIGN(MojoRenderer);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_RENDERER_H_
