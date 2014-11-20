// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/pipeline_status.h"

namespace media {
class AudioDecoderConfig;
class BrowserCdm;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class AudioPipeline;
class CodedFrameProvider;
struct MediaPipelineClient;
class VideoPipeline;

class MediaPipeline {
 public:
  MediaPipeline() {}
  virtual ~MediaPipeline() {}

  // Set the media pipeline client.
  virtual void SetClient(const MediaPipelineClient& client) = 0;

  // Set the CDM to use for decryption.
  // The CDM is refered by its id.
  virtual void SetCdm(int cdm_id) = 0;

  // Return the audio/video pipeline owned by the MediaPipeline.
  virtual AudioPipeline* GetAudioPipeline() const = 0;
  virtual VideoPipeline* GetVideoPipeline() const = 0;

  // Create an audio/video pipeline.
  // MediaPipeline owns the resulting audio/video pipeline.
  // Only one audio and one video pipeline can be created.
  virtual void InitializeAudio(
      const ::media::AudioDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb) = 0;
  virtual void InitializeVideo(
      const ::media::VideoDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb) = 0;

  // Control the media pipeline state machine.
  virtual void StartPlayingFrom(base::TimeDelta time) = 0;
  virtual void Flush(const ::media::PipelineStatusCB& status_cb) = 0;
  virtual void Stop() = 0;

  // Set the playback rate.
  virtual void SetPlaybackRate(float playback_rate) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaPipeline);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_H_
