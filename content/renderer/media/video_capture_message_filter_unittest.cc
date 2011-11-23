// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() {
    Reset();
    device_id_received_ = false;
    device_id_ = 0;
  }

  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length, int buffer_id) {
    buffer_created_ = true;
    handle_ = handle;
  }

  // Called when a video frame buffer is received from the browser process.
  virtual void OnBufferReceived(int buffer_id, base::Time timestamp) {
    buffer_received_ = true;
    buffer_id_ = buffer_id;
    timestamp_ = timestamp;
  }

  virtual void OnStateChanged(video_capture::State state) {
    state_changed_received_ = true;
    state_ = state;
  }

  virtual void OnDeviceInfoReceived(const media::VideoCaptureParams& params) {
    device_info_received_ = true;
    params_.width = params.width;
    params_.height = params.height;
    params_.frame_per_second = params.frame_per_second;
  }

  virtual void OnDelegateAdded(int32 device_id) {
    device_id_received_ = true;
    device_id_ = device_id;
  }

  void Reset() {
    buffer_created_ = false;
    handle_ = base::SharedMemory::NULLHandle();

    buffer_received_ = false;
    buffer_id_ = -1;
    timestamp_ = base::Time();

    state_changed_received_ = false;
    state_ = video_capture::kError;

    device_info_received_ = false;
    params_.width = 0;
    params_.height = 0;
    params_.frame_per_second = 0;
  }

  bool buffer_created() { return buffer_created_; }
  base::SharedMemoryHandle received_buffer_handle() { return handle_; }

  bool buffer_received() { return buffer_received_; }
  int received_buffer_id() { return buffer_id_; }
  base::Time received_buffer_ts() { return timestamp_; }

  bool state_changed_received() { return state_changed_received_; }
  video_capture::State state() { return state_; }

  bool device_info_receive() { return device_info_received_; }
  const media::VideoCaptureParams& received_device_info() { return params_; }

  bool device_id_received() { return device_id_received_; }
  int32 device_id() { return device_id_; }

 private:
  bool buffer_created_;
  base::SharedMemoryHandle handle_;

  bool buffer_received_;
  int buffer_id_;
  base::Time timestamp_;

  bool state_changed_received_;
  video_capture::State state_;

  bool device_info_received_;
  media::VideoCaptureParams params_;

  bool device_id_received_;
  int32 device_id_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureDelegate);
};

}  // namespace

TEST(VideoCaptureMessageFilterTest, Basic) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());
  filter->channel_ = reinterpret_cast<IPC::Channel*>(1);

  MockVideoCaptureDelegate delegate;
  filter->AddDelegate(&delegate);

  // VideoCaptureMsg_StateChanged
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate.device_id(),
                                   video_capture::kStarted));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(video_capture::kStarted == delegate.state());
  delegate.Reset();

  // VideoCaptureMsg_NewBuffer
  const base::SharedMemoryHandle handle =
#if defined(OS_WIN)
      reinterpret_cast<base::SharedMemoryHandle>(10);
#else
      base::SharedMemoryHandle(10, true);
#endif
  EXPECT_FALSE(delegate.buffer_created());
  filter->OnMessageReceived(VideoCaptureMsg_NewBuffer(
      delegate.device_id(), handle, 1, 1));
  EXPECT_TRUE(delegate.buffer_created());
  EXPECT_EQ(handle, delegate.received_buffer_handle());
  delegate.Reset();

  // VideoCaptureMsg_BufferReady
  int buffer_id = 1;
  base::Time timestamp = base::Time::FromInternalValue(1);

  EXPECT_FALSE(delegate.buffer_received());
  filter->OnMessageReceived(VideoCaptureMsg_BufferReady(
      delegate.device_id(), buffer_id, timestamp));
  EXPECT_TRUE(delegate.buffer_received());
  EXPECT_EQ(buffer_id, delegate.received_buffer_id());
  EXPECT_TRUE(timestamp == delegate.received_buffer_ts());
  delegate.Reset();

  // VideoCaptureMsg_DeviceInfo
  media::VideoCaptureParams params;
  params.width = 320;
  params.height = 240;
  params.frame_per_second = 30;

  EXPECT_FALSE(delegate.device_info_receive());
  filter->OnMessageReceived(VideoCaptureMsg_DeviceInfo(
      delegate.device_id(), params));
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

  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());
  filter->channel_ = reinterpret_cast<IPC::Channel*>(1);

  MockVideoCaptureDelegate delegate1;
  MockVideoCaptureDelegate delegate2;

  filter->AddDelegate(&delegate1);
  filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   video_capture::kStarted));
  EXPECT_TRUE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   video_capture::kStarted));
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_TRUE(delegate2.state_changed_received());
  delegate2.Reset();

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  EXPECT_FALSE(delegate1.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   video_capture::kStarted));
  EXPECT_FALSE(delegate1.state_changed_received());

  filter->RemoveDelegate(&delegate2);
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   video_capture::kStarted));
  EXPECT_FALSE(delegate2.state_changed_received());

  message_loop.RunAllPending();
}
