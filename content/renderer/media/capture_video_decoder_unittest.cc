// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrictMock;

static const media::VideoCaptureSessionId kVideoStreamId = 1;

ACTION_P(ReturnFrameFromRenderer, decoder) {
  decoder->ProduceVideoFrame(arg0);
}

ACTION_P3(CreateDataBufferFromCapture, decoder, vc_impl, data_buffer_number) {
  for (int i = 0; i < data_buffer_number; i++) {
    media::VideoCapture::VideoFrameBuffer* buffer;
    buffer = new media::VideoCapture::VideoFrameBuffer();
    buffer->width = arg1.width;
    buffer->height = arg1.height;
    int length = buffer->width * buffer->height * 3 / 2;
    buffer->memory_pointer = new uint8[length];
    buffer->buffer_size = length;
    decoder->OnBufferReady(vc_impl, buffer);
  }
}

ACTION(DeleteDataBuffer) {
  delete[] arg0->memory_pointer;
}

ACTION_P2(CaptureStopped, decoder, vc_impl) {
  decoder->OnStopped(vc_impl);
}

class MockVideoCaptureImpl : public VideoCaptureImpl {
 public:
  MockVideoCaptureImpl(const media::VideoCaptureSessionId id,
                       scoped_refptr<base::MessageLoopProxy> ml_proxy,
                       VideoCaptureMessageFilter* filter)
      : VideoCaptureImpl(id, ml_proxy, filter) {
  }
  virtual ~MockVideoCaptureImpl() {}

  MOCK_METHOD2(StartCapture,
               void(media::VideoCapture::EventHandler* handler,
                    const VideoCaptureCapability& capability));
  MOCK_METHOD1(StopCapture, void(media::VideoCapture::EventHandler* handler));
  MOCK_METHOD1(FeedBuffer, void(scoped_refptr<VideoFrameBuffer> buffer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImpl);
};

class MockVideoCaptureImplManager : public VideoCaptureImplManager {
 public:
  MockVideoCaptureImplManager() {}
  virtual ~MockVideoCaptureImplManager() {}

  MOCK_METHOD2(AddDevice,
               media::VideoCapture*(media::VideoCaptureSessionId id,
                   media::VideoCapture::EventHandler* handler));
  MOCK_METHOD2(RemoveDevice,
               void(media::VideoCaptureSessionId id,
                   media::VideoCapture::EventHandler* handler));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImplManager);
};

class CaptureVideoDecoderTest : public ::testing::Test {
 protected:
  CaptureVideoDecoderTest() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    message_loop_proxy_ =
        base::MessageLoopProxy::current().get();
    vc_manager_ = new MockVideoCaptureImplManager();
    media::VideoCapture::VideoCaptureCapability capability;
    capability.width = 176;
    capability.height = 144;
    capability.max_fps = 30;
    capability.expected_capture_delay = 0;
    capability.raw_type = media::VideoFrame::I420;
    capability.interlaced = false;
    capability.resolution_fixed = false;

    decoder_ = new CaptureVideoDecoder(message_loop_proxy_,
                                       kVideoStreamId, vc_manager_, capability);
    renderer_ = new media::MockVideoRenderer();

    decoder_->set_host(&host_);
    decoder_->set_consume_video_frame_callback(
        base::Bind(&media::MockVideoRenderer::ConsumeVideoFrame,
                   base::Unretained(renderer_.get())));
    EXPECT_CALL(statistics_callback_object_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual ~CaptureVideoDecoderTest() {
    message_loop_->RunAllPending();
  }

  media::StatisticsCallback NewStatisticsCallback() {
    return base::Bind(&media::MockStatisticsCallback::OnStatistics,
                      base::Unretained(&statistics_callback_object_));
  }

  // Fixture members.
  scoped_refptr<CaptureVideoDecoder> decoder_;
  scoped_refptr<MockVideoCaptureImplManager> vc_manager_;
  scoped_refptr<media::MockVideoRenderer> renderer_;
  media::MockStatisticsCallback statistics_callback_object_;
  StrictMock<media::MockFilterHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptureVideoDecoderTest);
};

TEST_F(CaptureVideoDecoderTest, Play) {
  int data_buffer_number = 1;
  media::VideoCapture::EventHandler* capture_client =
      static_cast<media::VideoCapture::EventHandler*>(decoder_);
  scoped_ptr<MockVideoCaptureImpl> vc_impl(
      new MockVideoCaptureImpl(kVideoStreamId,
                               message_loop_proxy_,
                               new VideoCaptureMessageFilter()));

  EXPECT_CALL(*vc_manager_, AddDevice(_, _))
      .WillOnce(Return(vc_impl.get()));
  decoder_->Initialize(NULL,
                       media::NewExpectedClosure(),
                       NewStatisticsCallback());
  message_loop_->RunAllPending();

  EXPECT_CALL(*renderer_, ConsumeVideoFrame(_))
      .WillRepeatedly(ReturnFrameFromRenderer(decoder_.get()));
  EXPECT_CALL(*vc_impl, StartCapture(capture_client, _))
      .Times(1)
      .WillOnce(CreateDataBufferFromCapture(capture_client, vc_impl.get(),
                                            data_buffer_number));
  EXPECT_CALL(*vc_impl, FeedBuffer(_))
      .Times(data_buffer_number)
      .WillRepeatedly(DeleteDataBuffer());
  decoder_->Seek(base::TimeDelta(),
                 media::NewExpectedStatusCB(media::PIPELINE_OK));
  decoder_->Play(media::NewExpectedClosure());
  message_loop_->RunAllPending();

  EXPECT_CALL(*vc_impl, StopCapture(capture_client))
      .Times(1)
      .WillOnce(CaptureStopped(capture_client, vc_impl.get()));
  EXPECT_CALL(*vc_manager_, RemoveDevice(_, _))
      .WillOnce(Return());
  decoder_->Stop(media::NewExpectedClosure());
  message_loop_->RunAllPending();
}
