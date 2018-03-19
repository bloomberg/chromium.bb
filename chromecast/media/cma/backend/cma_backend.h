// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CMA_BACKEND_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CMA_BACKEND_H_

#include <stdint.h>

#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

// Interface for the media backend used by the CMA pipeline. The implementation
// is selected by CmaBackendFactory. MediaPipelineBackend is a lower-level
// interface used to abstract the platform, with a separate implementation for
// each platform, while CmaBackend implementations are used across multiple
// platforms.
class CmaBackend {
 public:
  using BufferStatus = MediaPipelineBackend::BufferStatus;
  using Decoder = MediaPipelineBackend::Decoder;
  using VideoDecoder = MediaPipelineBackend::VideoDecoder;

  class AudioDecoder : public MediaPipelineBackend::AudioDecoder {
   public:
    // Returns true if the audio decoder requires that encrypted buffers be
    // decrypted before being passed to PushBuffer(). The return value may
    // change whenever SetConfig() is called or the backend is initialized.
    virtual bool RequiresDecryption() = 0;

   protected:
    ~AudioDecoder() override = default;
  };

  virtual ~CmaBackend() = default;

  // These methods have the same behavior as the corresponding methods on
  // MediaPipelineBackend. See chromecast/public/media/media_pipeline_backend.h
  // for documentation.
  virtual AudioDecoder* CreateAudioDecoder() = 0;
  virtual VideoDecoder* CreateVideoDecoder() = 0;
  virtual bool Initialize() = 0;
  virtual bool Start(int64_t start_pts) = 0;
  virtual void Stop() = 0;
  virtual bool Pause() = 0;
  virtual bool Resume() = 0;
  virtual int64_t GetCurrentPts() = 0;
  virtual bool SetPlaybackRate(float rate) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CMA_BACKEND_H_
