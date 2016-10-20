// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace content {
namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() : device_id_(0) {
    ON_CALL(*this, OnDelegateAdded(_))
        .WillByDefault(WithArgs<0>(
            Invoke(this, &MockVideoCaptureDelegate::DoOnDelegateAdded)));
  }

  MOCK_METHOD1(OnDelegateAdded, void(int32_t));
  void DoOnDelegateAdded(int32_t device_id) {
    ASSERT_NE(0, device_id);
    ASSERT_EQ(0, device_id_);
    device_id_ = device_id;
  }
  int device_id() { return device_id_; }

 private:
  int device_id_;
};

}  // anonymous namespace

TEST(VideoCaptureMessageFilterTest, Basic) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);
  MockVideoCaptureDelegate delegate;

  EXPECT_CALL(delegate, OnDelegateAdded(1));
  filter->AddDelegate(&delegate);
  ASSERT_EQ(1, delegate.device_id());

  Mock::VerifyAndClearExpectations(&delegate);
}

TEST(VideoCaptureMessageFilterTest, Delegates) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);

  MockVideoCaptureDelegate delegate1;
  MockVideoCaptureDelegate delegate2;

  EXPECT_CALL(delegate1, OnDelegateAdded(1));
  EXPECT_CALL(delegate2, OnDelegateAdded(2));
  filter->AddDelegate(&delegate1);
  filter->AddDelegate(&delegate2);
  ASSERT_EQ(1, delegate1.device_id());
  ASSERT_EQ(2, delegate2.device_id());

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  filter->RemoveDelegate(&delegate2);
}

}  // namespace content
