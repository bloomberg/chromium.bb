// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {
namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() : device_id_(0) {}

  // VideoCaptureMessageFilter::Delegate implementation.
  MOCK_METHOD3(OnBufferCreated, void(base::SharedMemoryHandle handle,
                                     int length,
                                     int buffer_id));
  MOCK_METHOD1(OnBufferDestroyed, void(int buffer_id));
  MOCK_METHOD3(OnBufferReceived, void(int buffer_id,
                                      base::Time timestamp,
                                      const media::VideoCaptureFormat& format));
  MOCK_METHOD1(OnStateChanged, void(VideoCaptureState state));

  virtual void OnDelegateAdded(int32 device_id) OVERRIDE {
    ASSERT_TRUE(device_id != 0);
    ASSERT_TRUE(device_id_ == 0);
    device_id_ = device_id;
  }

  int device_id() { return device_id_; }

 private:
  int device_id_;
};

}  // namespace

TEST(VideoCaptureMessageFilterTest, Basic) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);
  MockVideoCaptureDelegate delegate;
  filter->AddDelegate(&delegate);
  ASSERT_EQ(1, delegate.device_id());

  // VideoCaptureMsg_StateChanged
  EXPECT_CALL(delegate, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate);

  // VideoCaptureMsg_NewBuffer
  const base::SharedMemoryHandle handle =
#if defined(OS_WIN)
      reinterpret_cast<base::SharedMemoryHandle>(10);
#else
      base::SharedMemoryHandle(10, true);
#endif
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
  filter->OnMessageReceived(VideoCaptureMsg_NewBuffer(
      delegate.device_id(), handle, 100, 1));
  Mock::VerifyAndClearExpectations(&delegate);

  // VideoCaptureMsg_BufferReady
  int buffer_id = 22;
  base::Time timestamp = base::Time::FromInternalValue(1);

  media::VideoCaptureFormat format(
      gfx::Size(234, 512), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureFormat saved_format;
  EXPECT_CALL(delegate, OnBufferReceived(buffer_id, timestamp, _))
      .WillRepeatedly(SaveArg<2>(&saved_format));
  filter->OnMessageReceived(VideoCaptureMsg_BufferReady(
      delegate.device_id(), buffer_id, timestamp, format));
  Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_EQ(234, saved_format.frame_size.width());
  EXPECT_EQ(512, saved_format.frame_size.height());
  EXPECT_EQ(30, saved_format.frame_rate);

  // VideoCaptureMsg_FreeBuffer
  EXPECT_CALL(delegate, OnBufferDestroyed(buffer_id));
  filter->OnMessageReceived(VideoCaptureMsg_FreeBuffer(
      delegate.device_id(), buffer_id));
  Mock::VerifyAndClearExpectations(&delegate);
}

TEST(VideoCaptureMessageFilterTest, Delegates) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);

  StrictMock<MockVideoCaptureDelegate> delegate1;
  StrictMock<MockVideoCaptureDelegate> delegate2;

  filter->AddDelegate(&delegate1);
  filter->AddDelegate(&delegate2);
  ASSERT_EQ(1, delegate1.device_id());
  ASSERT_EQ(2, delegate2.device_id());

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_CALL(delegate1, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate1);

  EXPECT_CALL(delegate2, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate2);

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   VIDEO_CAPTURE_STATE_ENDED));

  filter->RemoveDelegate(&delegate2);
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   VIDEO_CAPTURE_STATE_ENDED));
}

}  // namespace content
