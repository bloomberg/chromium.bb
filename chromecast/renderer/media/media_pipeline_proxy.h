// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/media/cma/pipeline/media_pipeline.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "media/base/serial_runner.h"

namespace base {
class MessageLoopProxy;
}

namespace chromecast {
namespace media {
class AudioPipelineProxy;
class MediaChannelProxy;
class MediaPipelineProxyInternal;
class VideoPipelineProxy;

class MediaPipelineProxy : public MediaPipeline {
 public:
  MediaPipelineProxy(
      int render_frame_id,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      LoadType load_type);
  ~MediaPipelineProxy() override;

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

 private:
  void OnProxyFlushDone(const ::media::PipelineStatusCB& status_cb,
                        ::media::PipelineStatus status);

  base::ThreadChecker thread_checker_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  const int render_frame_id_;

  // CMA channel to convey IPC messages.
  scoped_refptr<MediaChannelProxy> const media_channel_proxy_;

  scoped_ptr<MediaPipelineProxyInternal> proxy_;

  bool has_audio_;
  bool has_video_;
  scoped_ptr<AudioPipelineProxy> audio_pipeline_;
  scoped_ptr<VideoPipelineProxy> video_pipeline_;
  scoped_ptr< ::media::SerialRunner> pending_callbacks_;

  base::WeakPtr<MediaPipelineProxy> weak_this_;
  base::WeakPtrFactory<MediaPipelineProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_PIPELINE_PROXY_H_