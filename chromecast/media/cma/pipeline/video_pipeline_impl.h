// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/video_pipeline.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"

namespace gfx {
class Size;
}

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class AvPipelineImpl;
class BrowserCdmCast;
class BufferingState;
class CodedFrameProvider;
class VideoPipelineDevice;

class VideoPipelineImpl : public VideoPipeline {
 public:
  // |buffering_controller| can be NULL.
  explicit VideoPipelineImpl(VideoPipelineDevice* video_device);
  ~VideoPipelineImpl() override;

  // Input port of the pipeline.
  void SetCodedFrameProvider(scoped_ptr<CodedFrameProvider> frame_provider);

  // Provide the CDM to use to decrypt samples.
  void SetCdm(BrowserCdmCast* media_keys);

  // Functions to control the state of the audio pipeline.
  void Initialize(
      const ::media::VideoDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb);
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);
  void Flush(const ::media::PipelineStatusCB& status_cb);
  void Stop();

  // Update the playback statistics for this video stream.
  void UpdateStatistics();

  // VideoPipeline implementation.
  void SetClient(const VideoPipelineClient& client) override;

 private:
  void OnFlushDone(const ::media::PipelineStatusCB& status_cb);
  void OnUpdateConfig(const ::media::AudioDecoderConfig& audio_config,
                      const ::media::VideoDecoderConfig& video_config);
  void OnNaturalSizeChanged(const gfx::Size& size);

  VideoPipelineDevice* video_device_;

  scoped_refptr<AvPipelineImpl> av_pipeline_impl_;
  VideoPipelineClient video_client_;

  ::media::PipelineStatistics previous_stats_;

  base::WeakPtr<VideoPipelineImpl> weak_this_;
  base::WeakPtrFactory<VideoPipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_
