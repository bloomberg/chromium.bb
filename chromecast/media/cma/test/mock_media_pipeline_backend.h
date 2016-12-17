// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_H_
#define CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_H_

#include "chromecast/public/media/media_pipeline_backend.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockAudioDecoder : public MediaPipelineBackend::AudioDecoder {
 public:
  MockAudioDecoder();
  ~MockAudioDecoder() override;

  MOCK_METHOD1(SetDelegate, void(Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(CastDecoderBuffer*));
  MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
  MOCK_METHOD1(SetVolume, bool(float));
  MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
  MOCK_METHOD1(GetStatistics, void(Statistics*));
};

class MockVideoDecoder : public MediaPipelineBackend::VideoDecoder {
 public:
  MockVideoDecoder();
  ~MockVideoDecoder() override;

  MOCK_METHOD1(SetDelegate, void(Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(CastDecoderBuffer*));
  MOCK_METHOD1(SetConfig, bool(const VideoConfig&));
  MOCK_METHOD1(GetStatistics, void(Statistics*));
};

class MockMediaPipelineBackend : public MediaPipelineBackend {
 public:
  MockMediaPipelineBackend();
  ~MockMediaPipelineBackend() override;

  MOCK_METHOD0(CreateAudioDecoder, AudioDecoder*());
  MOCK_METHOD0(CreateVideoDecoder, VideoDecoder*());
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD1(Start, bool(int64_t));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, bool());
  MOCK_METHOD0(Resume, bool());
  MOCK_METHOD0(GetCurrentPts, int64_t());
  MOCK_METHOD1(SetPlaybackRate, bool(float));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_H_
