// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/filters/cma_renderer.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/null_video_sink.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class CmaEndToEndTest : public testing::Test {
 public:
  CmaEndToEndTest() {}

  void SetUp() override {
    demuxer_stream_provider_.reset(
        new ::media::FakeDemuxerStreamProvider(1, 1, false));
    null_sink_.reset(new ::media::NullVideoSink(
        false, base::TimeDelta::FromSecondsD(1.0 / 60),
        base::Bind(&MockCB::OnFrameReceived, base::Unretained(&mock_)),
        message_loop_.task_runner()));

    scoped_ptr<MediaPipelineImpl> media_pipeline(new MediaPipelineImpl());
    task_runner_.reset(new TaskRunnerImpl());
    MediaPipelineDeviceParams params(task_runner_.get());
    scoped_ptr<MediaPipelineBackend> backend =
        make_scoped_ptr(new MediaPipelineBackendDefault(params));

    gles2_.reset(new gpu::gles2::GLES2InterfaceStub());
    mock_gpu_factories_ = new ::media::MockGpuVideoAcceleratorFactories();

    EXPECT_CALL(*mock_gpu_factories_.get(), GetGLES2Interface())
        .WillRepeatedly(testing::Return(gles2_.get()));

    media_pipeline->Initialize(kLoadTypeMediaSource, backend.Pass());

    renderer_.reset(new CmaRenderer(media_pipeline.Pass(), null_sink_.get(),
                                    mock_gpu_factories_));
  }
  void TearDown() override { message_loop_.RunUntilIdle(); }

  ~CmaEndToEndTest() override {}

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<TaskRunnerImpl> task_runner_;
  scoped_ptr<::media::FakeDemuxerStreamProvider> demuxer_stream_provider_;
  scoped_ptr<CmaRenderer> renderer_;
  scoped_refptr<::media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  scoped_ptr<gpu::gles2::GLES2Interface> gles2_;

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
}


}  // namespace media
}  // namespace chromecast
