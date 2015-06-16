// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device_factory_default.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/filters/cma_renderer.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/null_video_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class CmaEndToEndTest : public testing::Test {
 public:
  CmaEndToEndTest() {
    demuxer_stream_provider_.reset(
        new ::media::FakeDemuxerStreamProvider(1, 1, false));
    null_sink_.reset(new ::media::NullVideoSink(
        false, base::TimeDelta::FromSecondsD(1.0 / 60),
        base::Bind(&MockCB::OnFrameReceived, base::Unretained(&mock_)),
        message_loop_.task_runner()));

    scoped_ptr<MediaPipelineImpl> media_pipeline(new MediaPipelineImpl());
    scoped_ptr<MediaPipelineDeviceFactory> factory =
        make_scoped_ptr(new MediaPipelineDeviceFactoryDefault());

    media_pipeline->Initialize(
        kLoadTypeMediaSource,
        make_scoped_ptr(new MediaPipelineDevice(factory.Pass())));

    renderer_.reset(new CmaRenderer(media_pipeline.Pass(), null_sink_.get()));
  }

  ~CmaEndToEndTest() override {}

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<::media::FakeDemuxerStreamProvider> demuxer_stream_provider_;
  scoped_ptr<CmaRenderer> renderer_;

  class MockCB {
   public:
    MOCK_METHOD1(OnInitialized, void(::media::PipelineStatus));
    MOCK_METHOD1(OnFrameReceived,
                 void(const scoped_refptr<::media::VideoFrame>&));
    MOCK_METHOD1(OnStatistics, void(const ::media::PipelineStatistics&));
    MOCK_METHOD1(OnBufferingState, void(::media::BufferingState));
    MOCK_METHOD0(OnEnded, void());
    MOCK_METHOD1(OnError, void(::media::PipelineStatus));
    MOCK_METHOD0(OnWaitingForDecryptionKey, void());
  };
  MockCB mock_;

 private:
  scoped_ptr<::media::NullVideoSink> null_sink_;

  DISALLOW_COPY_AND_ASSIGN(CmaEndToEndTest);
};

}  // namespace

TEST_F(CmaEndToEndTest, TestInitialization) {
  renderer_->Initialize(
      demuxer_stream_provider_.get(),
      base::Bind(&MockCB::OnInitialized, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnStatistics, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnBufferingState, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnEnded, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnError, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnWaitingForDecryptionKey, base::Unretained(&mock_)));

  EXPECT_CALL(mock_, OnInitialized(::media::PIPELINE_OK));
  message_loop_.RunUntilIdle();
}


}  // namespace media
}  // namespace chromecast
