// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

#define DEFAULT_CAPABILITY_REGULAR {176, 144, 30, 0, media::VideoFrame::I420, \
    false, false }
#define DEFAULT_CAPABILITY_MASTER {320, 240, 20, 0, media::VideoFrame::I420, \
    false, true }

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

    // Override Send() to mimic device to send events.
    void Send(IPC::Message* message) {
      CHECK(message);

      // In this method, messages are sent to the according handlers as if
      // we are the device.
      bool handled = true;
      IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureImpl, *message)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, DeviceStartCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, DevicePauseCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, DeviceStopCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady,
                            DeviceReceiveEmptyBuffer)
        IPC_MESSAGE_UNHANDLED(handled = false)
      IPC_END_MESSAGE_MAP()
      EXPECT_TRUE(handled);
      delete message;
    }

    void DeviceStartCapture(int device_id,
                            const media::VideoCaptureParams& params) {
      media::VideoCaptureParams device_info = params;
      media::VideoCapture::State state = kStarted;
      OnDeviceInfoReceived(device_info);
      OnStateChanged(state);
    }

    void DevicePauseCapture(int device_id) {}

    void DeviceStopCapture(int device_id) {
      media::VideoCapture::State state = kStopped;
      OnStateChanged(state);
    }

    void DeviceReceiveEmptyBuffer(int device_id, int buffer_id) {}
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
  // Execute SetCapture() and StopCapture() for one client.
  scoped_ptr<MockVideoCaptureClient> client(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability =
      DEFAULT_CAPABILITY_REGULAR;

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), capability);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunAllPending();
}

TEST_F(VideoCaptureImplTest, TwoClientsInSequence) {
  // Execute SetCapture() and StopCapture() for 2 clients in sequence.
  scoped_ptr<MockVideoCaptureClient> client(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability =
      DEFAULT_CAPABILITY_REGULAR;

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), capability);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunAllPending();

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), capability);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunAllPending();
}

TEST_F(VideoCaptureImplTest, MasterAndRegular) {
  // Execute SetCapture() and StopCapture() for 2 clients simultaneously.
  // The master client starts first and stops first.
  scoped_ptr<MockVideoCaptureClient> client_regular(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client_master(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability_regular =
      DEFAULT_CAPABILITY_REGULAR;
  media::VideoCapture::VideoCaptureCapability capability_master =
      DEFAULT_CAPABILITY_MASTER;

  EXPECT_CALL(*client_master, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client_master.get(), capability_master);
  video_capture_impl_->StartCapture(client_regular.get(), capability_regular);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client_master, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client_master.get());
  video_capture_impl_->StopCapture(client_regular.get());
  message_loop_->RunAllPending();
}

TEST_F(VideoCaptureImplTest, RegularAndMaster) {
  // Execute SetCapture() and StopCapture() for 2 clients simultaneously.
  // The regular client starts first and stops first.
  scoped_ptr<MockVideoCaptureClient> client_regular(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client_master(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability_regular =
      DEFAULT_CAPABILITY_REGULAR;
  media::VideoCapture::VideoCaptureCapability capability_master =
      DEFAULT_CAPABILITY_MASTER;

  EXPECT_CALL(*client_master, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnDeviceInfoReceived(_,_))
      .Times(AtLeast(1))
      .WillRepeatedly(Return());

  video_capture_impl_->StartCapture(client_regular.get(), capability_regular);
  video_capture_impl_->StartCapture(client_master.get(), capability_master);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client_master, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_regular, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client_regular.get());
  video_capture_impl_->StopCapture(client_master.get());
  message_loop_->RunAllPending();
}

TEST_F(VideoCaptureImplTest, Master1AndMaster2) {
  // Execute SetCapture() and StopCapture() for 2 master clients simultaneously.
  // The two clients have different resolution. The second client is expected to
  // recevie an error.
  scoped_ptr<MockVideoCaptureClient> client_master1(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client_master2(new MockVideoCaptureClient);
  media::VideoCapture::VideoCaptureCapability capability1 =
      DEFAULT_CAPABILITY_MASTER;
  media::VideoCapture::VideoCaptureCapability capability2 =
      DEFAULT_CAPABILITY_MASTER;
  capability2.width++;

  EXPECT_CALL(*client_master1, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master1, OnDeviceInfoReceived(_,_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master2, OnError(_,_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master2, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client_master1.get(), capability1);
  video_capture_impl_->StartCapture(client_master2.get(), capability2);
  message_loop_->RunAllPending();

  EXPECT_CALL(*client_master1, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master1, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_master2, OnStopped(_))
      .Times(0);

  video_capture_impl_->StopCapture(client_master1.get());
  video_capture_impl_->StopCapture(client_master2.get());
  message_loop_->RunAllPending();
}
