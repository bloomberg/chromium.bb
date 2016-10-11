// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "media/base/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace content {
namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() : device_id_(0) {}

  // VideoCaptureMessageFilter::Delegate implementation.
  MOCK_METHOD3(OnBufferCreated,
               void(base::SharedMemoryHandle handle,
                    int length,
                    int buffer_id));
  MOCK_METHOD7(OnBufferReceived,
               void(int buffer_id,
                    base::TimeDelta timestamp,
                    const base::DictionaryValue& metadata,
                    media::VideoPixelFormat pixel_format,
                    media::VideoFrame::StorageType storage_type,
                    const gfx::Size& coded_size,
                    const gfx::Rect& visible_rect));

  void OnDelegateAdded(int32_t device_id) override {
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

  // VideoCaptureMsg_NewBuffer
#if defined(OS_WIN)
  HANDLE h = reinterpret_cast<HANDLE>(10);
  // Passing a process ID that is not the current process's to prevent
  // attachment brokering.
  const base::SharedMemoryHandle handle =
      base::SharedMemoryHandle(h, base::GetCurrentProcId() + 1);
  EXPECT_CALL(delegate,
              OnBufferCreated(
                  ::testing::Property(&base::SharedMemoryHandle::GetHandle, h),
                  100, 1));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  const base::SharedMemoryHandle handle =
      base::SharedMemoryHandle(10, 100, base::GetCurrentProcId());
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
#else
  const base::SharedMemoryHandle handle = base::SharedMemoryHandle(10, true);
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
#endif
  filter->OnMessageReceived(VideoCaptureMsg_NewBuffer(
      delegate.device_id(), handle, 100, 1));
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

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  filter->RemoveDelegate(&delegate2);
}

}  // namespace content
