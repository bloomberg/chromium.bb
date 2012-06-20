// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "content/renderer/media/capture_video_decoder.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_status.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrictMock;

static const int kWidth = 176;
static const int kHeight = 144;
static const int kFPS = 30;
static const media::VideoCaptureSessionId kVideoStreamId = 1;

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

  MOCK_METHOD2(StartCapture,
               void(media::VideoCapture::EventHandler* handler,
                    const media::VideoCaptureCapability& capability));
  MOCK_METHOD1(StopCapture, void(media::VideoCapture::EventHandler* handler));
  MOCK_METHOD1(FeedBuffer, void(scoped_refptr<VideoFrameBuffer> buffer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImpl);
};

class MockVideoCaptureImplManager : public VideoCaptureImplManager {
 public:
  MockVideoCaptureImplManager() {}

  MOCK_METHOD2(AddDevice,
               media::VideoCapture*(media::VideoCaptureSessionId id,
                   media::VideoCapture::EventHandler* handler));
  MOCK_METHOD2(RemoveDevice,
               void(media::VideoCaptureSessionId id,
                   media::VideoCapture::EventHandler* handler));

 protected:
  virtual ~MockVideoCaptureImplManager() {}

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
    media::VideoCaptureCapability capability;
    capability.width = kWidth;
    capability.height = kHeight;
    capability.frame_rate = kFPS;
    capability.expected_capture_delay = 0;
    capability.color = media::VideoCaptureCapability::kI420;
    capability.interlaced = false;

    decoder_ = new CaptureVideoDecoder(message_loop_proxy_,
                                       kVideoStreamId, vc_manager_, capability);
    EXPECT_CALL(statistics_cb_object_, OnStatistics(_))
        .Times(AnyNumber());

    read_cb_ = base::Bind(&CaptureVideoDecoderTest::FrameReady,
                          base::Unretained(this));

    vc_impl_.reset(new MockVideoCaptureImpl(
        kVideoStreamId, message_loop_proxy_, new VideoCaptureMessageFilter()));
  }

  virtual ~CaptureVideoDecoderTest() {
    message_loop_->RunAllPending();
  }

  media::StatisticsCB NewStatisticsCB() {
    return base::Bind(&media::MockStatisticsCB::OnStatistics,
                      base::Unretained(&statistics_cb_object_));
  }

  void Initialize() {
    // Issue a read.
    EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk, _));
    decoder_->Read(read_cb_);

    EXPECT_CALL(*vc_manager_, AddDevice(_, _))
        .WillOnce(Return(vc_impl_.get()));
    int buffer_count = 1;
    EXPECT_CALL(*vc_impl_, StartCapture(capture_client(), _))
        .Times(1)
        .WillOnce(CreateDataBufferFromCapture(capture_client(),
                                              vc_impl_.get(),
                                              buffer_count));
    EXPECT_CALL(*vc_impl_, FeedBuffer(_))
        .Times(buffer_count)
        .WillRepeatedly(DeleteDataBuffer());

    decoder_->Initialize(NULL,
                         media::NewExpectedStatusCB(media::PIPELINE_OK),
                         NewStatisticsCB());
    message_loop_->RunAllPending();
  }

  void Stop() {
    EXPECT_CALL(*vc_impl_, StopCapture(capture_client()))
        .Times(1)
        .WillOnce(CaptureStopped(capture_client(), vc_impl_.get()));
    EXPECT_CALL(*vc_manager_, RemoveDevice(_, _))
        .WillOnce(Return());
    decoder_->Stop(media::NewExpectedClosure());
    message_loop_->RunAllPending();
  }

  media::VideoCapture::EventHandler* capture_client() {
    return static_cast<media::VideoCapture::EventHandler*>(decoder_);
  }

  MOCK_METHOD2(FrameReady, void(media::VideoDecoder::DecoderStatus status,
                                const scoped_refptr<media::VideoFrame>&));

  // Fixture members.
  scoped_refptr<CaptureVideoDecoder> decoder_;
  scoped_refptr<MockVideoCaptureImplManager> vc_manager_;
  scoped_ptr<MockVideoCaptureImpl> vc_impl_;
  media::MockStatisticsCB statistics_cb_object_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  media::VideoDecoder::ReadCB read_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptureVideoDecoderTest);
};

TEST_F(CaptureVideoDecoderTest, ReadAndReset) {
  // Test basic initialize and teardown sequence.
  Initialize();
  // Natural size should be initialized to default capability.
  EXPECT_EQ(kWidth, decoder_->natural_size().width());
  EXPECT_EQ(kHeight, decoder_->natural_size().height());

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk, _));
  decoder_->Read(read_cb_);
  decoder_->Reset(media::NewExpectedClosure());
  message_loop_->RunAllPending();

  Stop();
}

TEST_F(CaptureVideoDecoderTest, OnDeviceInfoReceived) {
  // Test that natural size gets updated as device information is sent.
  Initialize();

  gfx::Size expected_size(kWidth * 2, kHeight * 2);

  media::VideoCaptureParams params;
  params.width = expected_size.width();
  params.height = expected_size.height();
  params.frame_per_second = kFPS;
  params.session_id = kVideoStreamId;

  decoder_->OnDeviceInfoReceived(vc_impl_.get(), params);
  message_loop_->RunAllPending();

  EXPECT_EQ(expected_size.width(), decoder_->natural_size().width());
  EXPECT_EQ(expected_size.height(), decoder_->natural_size().height());

  Stop();
}
