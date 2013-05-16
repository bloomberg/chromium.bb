// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include "base/bind.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX)
#include "media/video/capture/screen/screen_capturer_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

const int kTestSharedMemoryId = 123;

namespace media {

class ScreenCapturerTest : public testing::Test {
 public:
  webrtc::SharedMemory* CreateSharedMemory(size_t size);

 protected:
  scoped_ptr<ScreenCapturer> capturer_;
  MockMouseShapeObserver mouse_observer_;
  MockScreenCapturerCallback callback_;
};

class FakeSharedMemory : public webrtc::SharedMemory {
 public:
  FakeSharedMemory(char* buffer, size_t size)
    : SharedMemory(buffer, size, 0, kTestSharedMemoryId),
      buffer_(buffer) {
  }
  virtual ~FakeSharedMemory() {
    delete[] buffer_;
  }
 private:
  char* buffer_;
  DISALLOW_COPY_AND_ASSIGN(FakeSharedMemory);
};

webrtc::SharedMemory* ScreenCapturerTest::CreateSharedMemory(size_t size) {
  return new FakeSharedMemory(new char[size], size);
}

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_ = ScreenCapturer::Create();
  capturer_->SetMouseShapeObserver(&mouse_observer_);
  capturer_->Start(&callback_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  webrtc::DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));
  EXPECT_CALL(mouse_observer_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<webrtc::SharedMemory*>(NULL)));

  capturer_ = ScreenCapturer::Create();
  capturer_->Start(&callback_);
  capturer_->Capture(webrtc::DesktopRegion());

  ASSERT_TRUE(frame);
  EXPECT_GT(frame->size().width(), 0);
  EXPECT_GT(frame->size().height(), 0);
  EXPECT_GE(frame->stride(),
            frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel);
  EXPECT_TRUE(frame->shared_memory() == NULL);

  // Verify that the region contains whole screen.
  EXPECT_FALSE(frame->updated_region().is_empty());
  webrtc::DesktopRegion::Iterator it(frame->updated_region());
  ASSERT_TRUE(!it.IsAtEnd());
  EXPECT_TRUE(it.rect().equals(webrtc::DesktopRect::MakeSize(frame->size())));
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());

  delete frame;
}

#if defined(OS_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  webrtc::DesktopFrame* frame = NULL;
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .WillOnce(SaveArg<0>(&frame));
  EXPECT_CALL(mouse_observer_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &ScreenCapturerTest::CreateSharedMemory));

  capturer_ = ScreenCapturer::Create();
  capturer_->Start(&callback_);
  capturer_->Capture(webrtc::DesktopRegion());

  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);

  delete frame;
}

#endif  // defined(OS_WIN)

}  // namespace media
