// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/stream_id.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
struct Size;
namespace media {
class AvPipelineImpl;
class BrowserCdmCast;
class BufferingState;
class CodedFrameProvider;

class VideoPipelineImpl {
 public:
  VideoPipelineImpl(MediaPipelineBackend::VideoDecoder* decoder,
                    const VideoPipelineClient& client);
  ~VideoPipelineImpl();

  // Input port of the pipeline.
  void SetCodedFrameProvider(scoped_ptr<CodedFrameProvider> frame_provider);

  // Provide the CDM to use to decrypt samples.
  void SetCdm(BrowserCdmCast* media_keys);

  // Functions to control the state of the audio pipeline.
  void Initialize(
      const std::vector<::media::VideoDecoderConfig>& configs,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb);
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);
  void Flush(const ::media::PipelineStatusCB& status_cb);
  void BackendStopped();
  void Stop();

  // Update the playback statistics for this video stream.
  void UpdateStatistics();

  void OnBufferPushed(MediaPipelineBackend::BufferStatus status);
  void OnEndOfStream();
  void OnError();
  void OnNaturalSizeChanged(const Size& size);

 private:
  class DeviceClientImpl;
  friend class DeviceClientImpl;

  void OnFlushDone(const ::media::PipelineStatusCB& status_cb);
  void OnUpdateConfig(StreamId id,
                      const ::media::AudioDecoderConfig& audio_config,
                      const ::media::VideoDecoderConfig& video_config);

  MediaPipelineBackend::VideoDecoder* video_decoder_;

  scoped_ptr<AvPipelineImpl> av_pipeline_impl_;
  VideoPipelineClient video_client_;

  ::media::PipelineStatistics previous_stats_;

  base::WeakPtr<VideoPipelineImpl> weak_this_;
  base::WeakPtrFactory<VideoPipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_VIDEO_PIPELINE_IMPL_H_
