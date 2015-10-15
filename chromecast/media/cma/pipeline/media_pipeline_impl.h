// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/serial_runner.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}  // namespace media

namespace chromecast {
namespace media {
class AudioPipelineImpl;
struct AvPipelineClient;
struct VideoPipelineClient;
class BrowserCdmCast;
class BufferingController;
class CodedFrameProvider;
class VideoPipelineImpl;

class MediaPipelineImpl : public MediaPipelineBackend::Delegate {
 public:
  MediaPipelineImpl();
  ~MediaPipelineImpl() override;

  // Initialize the media pipeline: the pipeline is configured based on
  // |load_type|.
  void Initialize(LoadType load_type,
                  scoped_ptr<MediaPipelineBackend> media_pipeline_backend);

  void SetClient(const MediaPipelineClient& client);
  void SetCdm(int cdm_id);

  // MediaPipelineBackendDelegate implementation:
  void OnVideoResolutionChanged(MediaPipelineBackend::VideoDecoder* decoder,
                                const Size& size) override;
  void OnPushBufferComplete(MediaPipelineBackend::Decoder* decoder,
                            MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream(MediaPipelineBackend::Decoder* decoder) override;
  void OnDecoderError(MediaPipelineBackend::Decoder* decoder) override;

  void InitializeAudio(const ::media::AudioDecoderConfig& config,
                       const AvPipelineClient& client,
                       scoped_ptr<CodedFrameProvider> frame_provider,
                       const ::media::PipelineStatusCB& status_cb);
  void InitializeVideo(const std::vector< ::media::VideoDecoderConfig>& configs,
                       const VideoPipelineClient& client,
                       scoped_ptr<CodedFrameProvider> frame_provider,
                       const ::media::PipelineStatusCB& status_cb);
  void StartPlayingFrom(base::TimeDelta time);
  void Flush(const ::media::PipelineStatusCB& status_cb);
  void Stop();
  void SetPlaybackRate(double playback_rate);
  void SetVolume(float volume);

  void SetCdm(BrowserCdmCast* cdm);

 private:
  void StateTransition(const ::media::PipelineStatusCB& status_cb,
                       ::media::PipelineStatus status);

  // Invoked to notify about a change of buffering state.
  void OnBufferingNotification(bool is_buffering);

  void UpdateMediaTime();

  void OnError(::media::PipelineStatus error);

  base::ThreadChecker thread_checker_;

  MediaPipelineClient client_;

  scoped_ptr<BufferingController> buffering_controller_;

  // Interface with the underlying hardware media pipeline.
  scoped_ptr<MediaPipelineBackend> media_pipeline_backend_;
  MediaPipelineBackend::AudioDecoder* audio_decoder_;
  MediaPipelineBackend::VideoDecoder* video_decoder_;

  bool backend_initialized_;
  bool has_audio_;
  bool has_video_;
  scoped_ptr<AudioPipelineImpl> audio_pipeline_;
  scoped_ptr<VideoPipelineImpl> video_pipeline_;
  scoped_ptr< ::media::SerialRunner> pending_flush_callbacks_;

  // Whether or not the backend is currently paused.
  bool paused_;
  // Playback rate set by the upper layer.
  float target_playback_rate_;

  // The media time is retrieved at regular intervals.
  // Indicate whether time update is enabled.
  bool enable_time_update_;
  bool pending_time_update_task_;
  base::TimeDelta last_media_time_;

  // Used to make the statistics update period a multiplier of the time update
  // period.
  int statistics_rolling_counter_;

  base::WeakPtr<MediaPipelineImpl> weak_this_;
  base::WeakPtrFactory<MediaPipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_IMPL_H_
