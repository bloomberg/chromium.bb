// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chromecast/common/media/cma_messages.h"
#include "chromecast/common/media/shared_memory_chunk.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/filters/cma_renderer.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/pipeline/media_pipeline.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"
#include "chromecast/renderer/media/media_pipeline_proxy.h"
#include "ipc/ipc_test_sink.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/null_video_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class CmaRendererTest : public testing::Test {
 public:
  CmaRendererTest() {
    demuxer_stream_provider_.reset(
        new ::media::FakeDemuxerStreamProvider(1, 1, false));
    null_sink_.reset(new ::media::NullVideoSink(
        false,
        base::TimeDelta::FromSecondsD(1.0 / 60),
        base::Bind(&MockCB::OnFrameReceived, base::Unretained(&mock_)),
        message_loop_.task_runner()));

    cma_proxy_ = new CmaMessageFilterProxy(message_loop_.task_runner());
    cma_proxy_->OnFilterAdded(&ipc_sink_);

    renderer_.reset(new CmaRenderer(
        scoped_ptr<MediaPipelineProxy>(new MediaPipelineProxy(
            0, message_loop_.task_runner(), LoadType::kLoadTypeMediaSource)),
        null_sink_.get()));
  }

  ~CmaRendererTest() override {
    cma_proxy_->OnFilterRemoved();
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<::media::FakeDemuxerStreamProvider> demuxer_stream_provider_;
  IPC::TestSink ipc_sink_;
  scoped_refptr<CmaMessageFilterProxy> cma_proxy_;
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

  DISALLOW_COPY_AND_ASSIGN(CmaRendererTest);
};

}  // namespace

TEST_F(CmaRendererTest, TestInitialization) {
  renderer_->Initialize(
      demuxer_stream_provider_.get(),
      base::Bind(&MockCB::OnInitialized, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnStatistics, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnBufferingState, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnEnded, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnError, base::Unretained(&mock_)),
      base::Bind(&MockCB::OnWaitingForDecryptionKey, base::Unretained(&mock_)));
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(ipc_sink_.GetUniqueMessageMatching(CmaHostMsg_CreateAvPipe::ID));
  base::SharedMemory* shared_memory = new base::SharedMemory();
  EXPECT_TRUE(shared_memory->CreateAndMapAnonymous(kAppVideoBufferSize));

  // Renderer MediaMessageFifo instance expects the first bytes of the shared
  // memory blob to describe how much space is available for writing.
  *(static_cast<size_t*>(shared_memory->memory())) =
      shared_memory->requested_size() - MediaMessageFifo::kDescriptorSize;

  base::FileDescriptor foreign_socket_handle;
  cma_proxy_->OnMessageReceived(CmaMsg_AvPipeCreated(
       1, kVideoTrackId, true,
       shared_memory->handle(), foreign_socket_handle));
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(
      ipc_sink_.GetUniqueMessageMatching(CmaHostMsg_AudioInitialize::ID));
  EXPECT_TRUE(
      ipc_sink_.GetUniqueMessageMatching(CmaHostMsg_VideoInitialize::ID));

  EXPECT_CALL(mock_, OnInitialized(::media::PIPELINE_OK));
  cma_proxy_->OnMessageReceived(
      CmaMsg_TrackStateChanged(1, kVideoTrackId, ::media::PIPELINE_OK));
  message_loop_.RunUntilIdle();
}


}  // namespace media
}  // namespace chromecast
