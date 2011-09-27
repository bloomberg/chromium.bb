// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "content/renderer/media/video_capture_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

#define DEFAULT_CAPABILITY {176, 144, 30, 0, media::VideoFrame::I420, \
    false, false }

ACTION(DeleteMessage) {
  delete arg0;
}

class MockVideoCaptureMessageFilter : public VideoCaptureMessageFilter {
 public:
  MockVideoCaptureMessageFilter() : VideoCaptureMessageFilter() {}
  virtual ~MockVideoCaptureMessageFilter() {}

  // Filter implementation.
  MOCK_METHOD1(Send, bool(IPC::Message* message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureMessageFilter);
};

class MockVideoCaptureClient : public media::VideoCapture::EventHandler {
 public:
  MockVideoCaptureClient() {}
  virtual ~MockVideoCaptureClient() {}

  // EventHandler implementation.
  MOCK_METHOD1(OnStarted, void(media::VideoCapture* capture));
  MOCK_METHOD1(OnStopped, void(media::VideoCapture* capture));
  MOCK_METHOD1(OnPaused, void(media::VideoCapture* capture));
  MOCK_METHOD2(OnError, void(media::VideoCapture* capture, int error_code));
  MOCK_METHOD1(OnRemoved, void(media::VideoCapture* capture));
  MOCK_METHOD2(OnBufferReady,
               void(media::VideoCapture* capture,
                    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf));
  MOCK_METHOD2(OnDeviceInfoReceived,
               void(media::VideoCapture* capture,
                    const media::VideoCaptureParams& device_info));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureClient);
};

class VideoCaptureImplTest : public ::testing::Test {
 public:
  class MockVideoCaptureImpl : public VideoCaptureImpl {
   public:
    MockVideoCaptureImpl(const media::VideoCaptureSessionId id,
                         scoped_refptr<base::MessageLoopProxy> ml_proxy,
                         VideoCaptureMessageFilter* filter)
        : VideoCaptureImpl(id, ml_proxy, filter) {
    }
    virtual ~MockVideoCaptureImpl() {}

    MOCK_METHOD1(Send, void(IPC::Message* message));
  };

  VideoCaptureImplTest() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    message_loop_proxy_ =
        base::MessageLoopProxy::current().get();

    message_filter_ = new MockVideoCaptureMessageFilter;
    session_id_ = 1;

    video_capture_impl_ = new MockVideoCaptureImpl(session_id_,
                                                   message_loop_proxy_,
                                                   message_filter_);

    video_capture_impl_->device_id_ = 2;
  }

  virtual ~VideoCaptureImplTest() {
    delete video_capture_impl_;
  }

 protected:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<MockVideoCaptureMessageFilter> message_filter_;
  media::VideoCaptureSessionId session_id_;
  MockVideoCaptureImpl* video_capture_impl_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplTest);
};

TEST_F(VideoCaptureImplTest, Simple) {
  // Execute SetCapture() and StopCapture().

  scoped_ptr<MockVideoCaptureClient> client(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability = DEFAULT_CAPABILITY;

  EXPECT_CALL(*video_capture_impl_, Send(_))
      .WillRepeatedly(DeleteMessage());

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), capability);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunAllPending();
}
