// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTE_RENDERER_IMPL_H_
#define MEDIA_REMOTING_REMOTE_RENDERER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "media/remoting/metrics.h"
#include "media/remoting/remoting_interstitial_ui.h"
#include "media/remoting/rpc/rpc_broker.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

class RemotingRendererController;
class Renderer;

namespace remoting {
class RemoteDemuxerStreamAdapter;
};

// A media::Renderer implementation that use a media::Renderer to render
// media streams.
class RemoteRendererImpl : public Renderer {
 public:
  // The whole class except for constructor and GetMediaTime() runs on
  // |media_task_runner|. The constructor and GetMediaTime() run on render main
  // thread.
  RemoteRendererImpl(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      const base::WeakPtr<RemotingRendererController>&
          remoting_renderer_controller,
      VideoRendererSink* video_renderer_sink);
  ~RemoteRendererImpl() final;

 private:
  // Callback when attempting to establish data pipe. The function is set to
  // static in order to post task to media thread in order to avoid threading
  // race condition.
  static void OnDataPipeCreatedOnMainThread(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      base::WeakPtr<RemoteRendererImpl> self,
      base::WeakPtr<remoting::RpcBroker> rpc_broker,
      mojom::RemotingDataStreamSenderPtrInfo audio,
      mojom::RemotingDataStreamSenderPtrInfo video,
      mojo::ScopedDataPipeProducerHandle audio_handle,
      mojo::ScopedDataPipeProducerHandle video_handle);

  // Callback function when RPC message is received. The function is set to
  // static in order to post task to media thread in order to avoid threading
  // race condition.
  static void OnMessageReceivedOnMainThread(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      base::WeakPtr<RemoteRendererImpl> self,
      std::unique_ptr<remoting::pb::RpcMessage> message);

  // Callback when remoting interstitial needs to be updated. Will post task to
  // media thread to avoid threading race condition.
  static void RequestUpdateInterstitialOnMainThread(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      base::WeakPtr<RemoteRendererImpl> remote_renderer_impl,
      const base::Optional<SkBitmap>& background_image,
      const gfx::Size& canvas_size,
      RemotingInterstitialType interstitial_type);

 public:
  // media::Renderer implementation.
  void Initialize(DemuxerStreamProvider* demuxer_stream_provider,
                  media::RendererClient* client,
                  const PipelineStatusCB& init_cb) final;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) final;
  void Flush(const base::Closure& flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;

 private:
  friend class RemoteRendererImplTest;

  enum State {
    STATE_UNINITIALIZED,
    STATE_CREATE_PIPE,
    STATE_ACQUIRING,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  // Callback when attempting to establish data pipe. Runs on media thread only.
  void OnDataPipeCreated(mojom::RemotingDataStreamSenderPtrInfo audio,
                         mojom::RemotingDataStreamSenderPtrInfo video,
                         mojo::ScopedDataPipeProducerHandle audio_handle,
                         mojo::ScopedDataPipeProducerHandle video_handle,
                         int audio_rpc_handle,
                         int video_rpc_handle);

  // Callback function when RPC message is received. Runs on media thread only.
  void OnReceivedRpc(std::unique_ptr<remoting::pb::RpcMessage> message);

  // Function to post task to main thread in order to send RPC message.
  void SendRpcToRemote(std::unique_ptr<remoting::pb::RpcMessage> message);

  // Functions when RPC message is received.
  void AcquireRendererDone(std::unique_ptr<remoting::pb::RpcMessage> message);
  void InitializeCallback(std::unique_ptr<remoting::pb::RpcMessage> message);
  void FlushUntilCallback();
  void SetCdmCallback(std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnTimeUpdate(std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnBufferingStateChange(
      std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnVideoNaturalSizeChange(
      std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnVideoOpacityChange(std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnStatisticsUpdate(std::unique_ptr<remoting::pb::RpcMessage> message);
  void OnDurationChange(std::unique_ptr<remoting::pb::RpcMessage> message);

  // Called to update the remoting interstitial. Update
  // |interstitial_background_| if |background_image| is set.
  void UpdateInterstitial(const base::Optional<SkBitmap>& background_image,
                          const gfx::Size& canvas_size,
                          RemotingInterstitialType interstitial_type);

  // Called when |current_media_time_| is updated.
  void OnMediaTimeUpdated();

  // Called to update the |video_stats_queue_|.
  void UpdateVideoStatsQueue(int video_frames_decoded,
                             int video_frames_dropped);

  // Called to clear all recent measurements history and schedule resuming after
  // a stabilization period elapses.
  void ResetMeasurements();

  // Called when a fatal runtime error occurs. |stop_trigger| is the error code
  // handed to the RemotingRendererController.
  void OnFatalError(remoting::StopTrigger stop_trigger);

  // Called periodically to measure the data flows from the
  // DemuxerStreamAdapters and record this information in the metrics.
  void MeasureAndRecordDataRates();

  State state_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Current renderer playback time information.
  base::TimeDelta current_media_time_;
  base::TimeDelta current_max_time_;
  // Both |current_media_time_| and |current_max_time_| should be protected by
  // lock because it can be accessed from both media and render main thread.
  base::Lock time_lock_;

  DemuxerStreamProvider* demuxer_stream_provider_;
  media::RendererClient* client_;
  std::unique_ptr<remoting::RemoteDemuxerStreamAdapter>
      audio_demuxer_stream_adapter_;
  std::unique_ptr<remoting::RemoteDemuxerStreamAdapter>
      video_demuxer_stream_adapter_;

  // Component to establish mojo remoting service on browser process.
  const base::WeakPtr<RemotingRendererController> remoting_renderer_controller_;
  // Broker class to process incoming and outgoing RPC message.
  const base::WeakPtr<remoting::RpcBroker> rpc_broker_;
  // RPC handle value for RemoteRendererImpl component.
  const int rpc_handle_;

  // RPC handle value for render on receiver endpoint.
  int remote_renderer_handle_;

  // Callbacks.
  PipelineStatusCB init_workflow_done_callback_;
  CdmAttachedCB cdm_attached_cb_;
  base::Closure flush_cb_;

  VideoRendererSink* const video_renderer_sink_;  // Outlives this class.
  // The background image for remoting interstitial. When |this| is destructed,
  // |interstitial_background_| will be paint to clear the cast messages on
  // the interstitial.
  SkBitmap interstitial_background_;
  gfx::Size canvas_size_;

  // Current playback rate.
  double playback_rate_ = 0;

  // Ignores updates until this time.
  base::TimeTicks ignore_updates_until_time_;

  // Indicates whether stats has been updated.
  bool stats_updated_ = false;

  // Stores all |current_media_time_| and the local time when updated in the
  // moving time window. This is used to check whether the playback duration
  // matches the update duration in the window.
  std::deque<std::pair<base::TimeTicks, base::TimeDelta>> media_time_queue_;

  // Stores all updates on the number of video frames decoded/dropped, and the
  // local time when updated in the moving time window. This is used to check
  // whether too many video frames were dropped.
  std::deque<std::tuple<base::TimeTicks, int, int>> video_stats_queue_;

  // The total number of frames decoded/dropped in the time window.
  int sum_video_frames_decoded_ = 0;
  int sum_video_frames_dropped_ = 0;

  // Records the number of consecutive times that remoting playback was delayed.
  int times_playback_delayed_ = 0;

  // Records events and measurements of interest.
  remoting::RendererMetricsRecorder metrics_recorder_;

  // A timer that polls the RemoteDemuxerStreamAdapters periodically to measure
  // the data flow rates for metrics.
  base::RepeatingTimer data_flow_poll_timer_;

  base::WeakPtrFactory<RemoteRendererImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteRendererImpl);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTE_RENDERER_IMPL_H_
