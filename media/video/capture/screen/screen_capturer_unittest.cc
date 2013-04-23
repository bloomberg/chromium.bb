// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include "base/bind.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX)
#include "media/video/capture/screen/screen_capture_data.h"
#include "media/video/capture/screen/screen_capturer_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace media {

MATCHER(DirtyRegionIsNonEmptyRect, "") {
  const SkRegion& dirty_region = arg->dirty_region();
  const SkIRect& dirty_region_bounds = dirty_region.getBounds();
  if (dirty_region_bounds.isEmpty()) {
    return false;
  }
  return dirty_region == SkRegion(dirty_region_bounds);
}

class ScreenCapturerTest : public testing::Test {
 public:
  scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size);

 protected:
  scoped_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerDelegate delegate_;
};

scoped_refptr<SharedBuffer> ScreenCapturerTest::CreateSharedBuffer(
    uint32 size) {
  return scoped_refptr<SharedBuffer>(new SharedBuffer(size));
}

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_ = ScreenCapturer::Create();
  capturer_->Start(&delegate_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  EXPECT_CALL(delegate_,
              OnCaptureCompleted(DirtyRegionIsNonEmptyRect()));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(delegate_, CreateSharedBuffer(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(scoped_refptr<SharedBuffer>()));

  capturer_ = ScreenCapturer::Create();
  capturer_->Start(&delegate_);
  capturer_->CaptureFrame();
}

#if defined(OS_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  EXPECT_CALL(delegate_,
              OnCaptureCompleted(DirtyRegionIsNonEmptyRect()));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(delegate_, CreateSharedBuffer(_))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &ScreenCapturerTest::CreateSharedBuffer));
  EXPECT_CALL(delegate_, ReleaseSharedBuffer(_))
      .Times(AnyNumber());

  capturer_ = ScreenCapturer::Create();
  capturer_->Start(&delegate_);
  capturer_->CaptureFrame();
  capturer_.reset();
}

#endif  // defined(OS_WIN)

}  // namespace media
