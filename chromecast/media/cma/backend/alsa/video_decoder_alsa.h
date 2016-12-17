// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_VIDEO_DECODER_ALSA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_VIDEO_DECODER_ALSA_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

class VideoDecoderAlsa : public MediaPipelineBackend::VideoDecoder {
 public:
  VideoDecoderAlsa();
  ~VideoDecoderAlsa() override;

  // MediaPipelineBackend::VideoDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const VideoConfig& config) override;

 private:
  void OnEndOfStream();

  Delegate* delegate_;
  base::WeakPtrFactory<VideoDecoderAlsa> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderAlsa);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_VIDEO_DECODER_ALSA_H_
