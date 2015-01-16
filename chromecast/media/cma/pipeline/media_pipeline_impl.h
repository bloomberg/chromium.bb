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
#include "chromecast/media/cma/pipeline/media_pipeline.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "media/base/serial_runner.h"

namespace chromecast {
namespace media {
class AudioPipelineImpl;
class BufferingController;
class MediaClockDevice;
class MediaPipelineDevice;
class VideoPipelineImpl;

class MediaPipelineImpl : public MediaPipeline {
 public:
  MediaPipelineImpl();
  ~MediaPipelineImpl() override;

  // Initialize the media pipeline: the pipeline is configured based on
  // |load_type|.
  void Initialize(LoadType load_type,
                  scoped_ptr<MediaPipelineDevice> media_pipeline_device);

  // MediaPipeline implementation.
  void SetClient(const MediaPipelineClient& client) override;
  void SetCdm(int cdm_id) override;
  AudioPipeline* GetAudioPipeline() const override;
  VideoPipeline* GetVideoPipeline() const override;
  void InitializeAudio(
      const ::media::AudioDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb) override;
  void InitializeVideo(
      const ::media::VideoDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void Flush(const ::media::PipelineStatusCB& status_cb) override;
  void Stop() override;
  void SetPlaybackRate(float playback_rate) override;

  AudioPipelineImpl* GetAudioPipelineImpl() const;
  VideoPipelineImpl* GetVideoPipelineImpl() const;

  void SetCdm(::media::BrowserCdm* cdm);

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
  scoped_ptr<MediaPipelineDevice> media_pipeline_device_;
  MediaClockDevice* clock_device_;

  bool has_audio_;
  bool has_video_;
  scoped_ptr<AudioPipelineImpl> audio_pipeline_;
  scoped_ptr<VideoPipelineImpl> video_pipeline_;
  scoped_ptr< ::media::SerialRunner> pending_callbacks_;

  // Playback rate set by the upper layer.
  float target_playback_rate_;

  // Indicate a possible re-buffering phase.
  bool is_buffering_;

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
