// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_AUDIO_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_AUDIO_PIPELINE_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/stream_id.h"

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

class AudioPipelineImpl {
 public:
  AudioPipelineImpl(MediaPipelineBackend::AudioDecoder* decoder,
                    const AvPipelineClient& client);
  ~AudioPipelineImpl();

  // Input port of the pipeline.
  void SetCodedFrameProvider(scoped_ptr<CodedFrameProvider> frame_provider);

  // Provide the CDM to use to decrypt samples.
  void SetCdm(BrowserCdmCast* media_keys);

  // Functions to control the state of the audio pipeline.
  void Initialize(
      const ::media::AudioDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb);
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);
  void Flush(const ::media::PipelineStatusCB& status_cb);
  void Stop();

  // Update the playback statistics for this audio stream.
  void UpdateStatistics();

  void SetVolume(float volume);

  void OnBufferPushed(MediaPipelineBackend::BufferStatus status);
  void OnEndOfStream();
  void OnError();

 private:
  void OnFlushDone(const ::media::PipelineStatusCB& status_cb);
  void OnUpdateConfig(StreamId id,
                      const ::media::AudioDecoderConfig& audio_config,
                      const ::media::VideoDecoderConfig& video_config);

  MediaPipelineBackend::AudioDecoder* decoder_;

  scoped_ptr<AvPipelineImpl> av_pipeline_impl_;
  AvPipelineClient audio_client_;

  ::media::PipelineStatistics previous_stats_;

  base::WeakPtr<AudioPipelineImpl> weak_this_;
  base::WeakPtrFactory<AudioPipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_AUDIO_PIPELINE_IMPL_H_
