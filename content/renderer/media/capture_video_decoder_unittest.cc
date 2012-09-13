// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "content/common/child_process.h"
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

ACTION(DeleteDataBuffer) {
  delete[] arg0->memory_pointer;
}

ACTION_P2(CaptureStopped, decoder, vc_impl) {
  decoder->OnStopped(vc_impl);
}

MATCHER_P2(HasSize, width, height, "") {
  EXPECT_EQ(arg->data_size().width(), width);
  EXPECT_EQ(arg->data_size().height(), height);
  EXPECT_EQ(arg->natural_size().width(), width);
  EXPECT_EQ(arg->natural_size().height(), height);
  return (arg->data_size().width() == width) &&
      (arg->data_size().height() == height) &&
      (arg->natural_size().width() == width) &&
      (arg->natural_size().height() == height);
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

    child_process_.reset(new ChildProcess());
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
    EXPECT_CALL(*vc_manager_, AddDevice(_, _))
        .WillOnce(Return(vc_impl_.get()));
    EXPECT_CALL(*vc_impl_, StartCapture(capture_client(), _));
    decoder_->Initialize(NULL,
                         media::NewExpectedStatusCB(media::PIPELINE_OK),
                         NewStatisticsCB());

    EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                  HasSize(kWidth, kHeight)));
        decoder_->Read(read_cb_);
    SendBufferToDecoder(gfx::Size(kWidth, kHeight));
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

  void SendBufferToDecoder(const gfx::Size& size) {
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer =
        new media::VideoCapture::VideoFrameBuffer();
    buffer->width = size.width();
    buffer->height = size.height();
    int length = buffer->width * buffer->height * 3 / 2;
    buffer->memory_pointer = new uint8[length];
    buffer->buffer_size = length;

    EXPECT_CALL(*vc_impl_, FeedBuffer(_))
        .WillOnce(DeleteDataBuffer());
    decoder_->OnBufferReady(vc_impl_.get(), buffer);
  }

  MOCK_METHOD2(FrameReady, void(media::VideoDecoder::Status status,
                                const scoped_refptr<media::VideoFrame>&));

  // Fixture members.
  scoped_refptr<CaptureVideoDecoder> decoder_;
  scoped_refptr<MockVideoCaptureImplManager> vc_manager_;
  scoped_ptr<ChildProcess> child_process_;
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
  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                HasSize(kWidth, kHeight)));
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

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk,
                                HasSize(expected_size.width(),
                                        expected_size.height())));
  decoder_->Read(read_cb_);
  SendBufferToDecoder(expected_size);
  message_loop_->RunAllPending();

  Stop();
}

TEST_F(CaptureVideoDecoderTest, ReadAndShutdown) {
  // Test all the Read requests can be fullfilled (which is needed in order to
  // teardown the pipeline) even when there's no input frame.
  Initialize();

  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk, HasSize(0, 0)));
  decoder_->Read(read_cb_);
  Stop();

  // Any read after stopping should be immediately satisfied.
  EXPECT_CALL(*this, FrameReady(media::VideoDecoder::kOk, HasSize(0, 0)));
  decoder_->Read(read_cb_);
  message_loop_->RunAllPending();
}
