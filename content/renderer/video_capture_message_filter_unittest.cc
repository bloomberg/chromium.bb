// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "content/common/video_capture_messages.h"
#include "content/renderer/video_capture_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() {
    Reset();
  }

  virtual void OnBufferReceived(TransportDIB::Handle handle,
                                base::Time timestamp) {
    buffer_received_ = true;
    handle_ = handle;
    timestamp_ = timestamp;
  }

  virtual void OnStateChanged(const media::VideoCapture::State& state) {
    state_changed_received_ = true;
    state_ = state;
  }

  virtual void OnDeviceInfoReceived(const media::VideoCaptureParams& params) {
    device_info_received_ = true;
    params_.width = params.width;
    params_.height = params.height;
    params_.frame_per_second = params.frame_per_second;
  }

  void Reset() {
    buffer_received_ = false;
    handle_ = TransportDIB::DefaultHandleValue();
    timestamp_ = base::Time();

    state_changed_received_ = false;
    state_ = media::VideoCapture::kError;

    device_info_received_ = false;
    params_.width = 0;
    params_.height = 0;
    params_.frame_per_second = 0;
  }

  bool buffer_received() { return buffer_received_; }
  TransportDIB::Handle received_buffer_handle() { return handle_; }
  base::Time received_buffer_ts() { return timestamp_; }

  bool state_changed_received() { return state_changed_received_; }
  media::VideoCapture::State state() { return state_; }

  bool device_info_receive() { return device_info_received_; }
  const media::VideoCaptureParams& received_device_info() { return params_; }

 private:
  bool buffer_received_;
  TransportDIB::Handle handle_;
  base::Time timestamp_;

  bool state_changed_received_;
  media::VideoCapture::State state_;

  bool device_info_received_;
  media::VideoCaptureParams params_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureDelegate);
};

}  // namespace

TEST(VideoCaptureMessageFilterTest, Basic) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter(kRouteId));

  MockVideoCaptureDelegate delegate;
  int device_id = filter->AddDelegate(&delegate);

  // VideoCaptureMsg_StateChanged
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId, device_id,
                                   media::VideoCapture::kStarted));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(media::VideoCapture::kStarted == delegate.state());
  delegate.Reset();

  // VideoCaptureMsg_BufferReady
  const TransportDIB::Handle handle = TransportDIB::GetFakeHandleForTest();
  base::Time timestamp = base::Time::FromInternalValue(1);

  EXPECT_FALSE(delegate.buffer_received());
  filter->OnMessageReceived(VideoCaptureMsg_BufferReady(
      kRouteId, device_id, handle, timestamp));
  EXPECT_TRUE(delegate.buffer_received());
#if defined(OS_MACOSX)
  EXPECT_EQ(handle.fd, delegate.received_buffer_handle().fd);
#else
  EXPECT_EQ(handle, delegate.received_buffer_handle());
#endif  // defined(OS_MACOSX)
  EXPECT_TRUE(timestamp == delegate.received_buffer_ts());
  delegate.Reset();

  // VideoCaptureMsg_DeviceInfo
  media::VideoCaptureParams params;
  params.width = 320;
  params.height = 240;
  params.frame_per_second = 30;

  EXPECT_FALSE(delegate.device_info_receive());
  filter->OnMessageReceived(VideoCaptureMsg_DeviceInfo(
      kRouteId, device_id, params));
  EXPECT_TRUE(delegate.device_info_receive());
  EXPECT_EQ(params.width, delegate.received_device_info().width);
  EXPECT_EQ(params.height, delegate.received_device_info().height);
  EXPECT_EQ(params.frame_per_second,
            delegate.received_device_info().frame_per_second);
  delegate.Reset();

  message_loop.RunAllPending();
}

TEST(VideoCaptureMessageFilterTest, Delegates) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter(kRouteId));

  MockVideoCaptureDelegate delegate1;
  MockVideoCaptureDelegate delegate2;

  int device_id1 = filter->AddDelegate(&delegate1);
  int device_id2 = filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId, device_id1,
                                   media::VideoCapture::kStarted));
  EXPECT_TRUE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId, device_id2,
                                   media::VideoCapture::kStarted));
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_TRUE(delegate2.state_changed_received());
  delegate2.Reset();

  // Send a message of a different route id, a message is not received.
  EXPECT_FALSE(delegate1.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId+1, device_id1,
                                   media::VideoCapture::kStarted));
  EXPECT_FALSE(delegate1.state_changed_received());

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  EXPECT_FALSE(delegate1.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId, device_id1,
                                   media::VideoCapture::kStarted));
  EXPECT_FALSE(delegate1.state_changed_received());

  filter->RemoveDelegate(&delegate2);
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(kRouteId, device_id2,
                                   media::VideoCapture::kStarted));
  EXPECT_FALSE(delegate2.state_changed_received());

  message_loop.RunAllPending();
}
