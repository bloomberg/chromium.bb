// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include <ApplicationServices/ApplicationServices.h>

#include <ostream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/video/capture/screen/mac/desktop_configuration.h"
#include "media/video/capture/screen/screen_capturer_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace media {

class ScreenCapturerMacTest : public testing::Test {
 public:
  // Verifies that the whole screen is initially dirty.
  void CaptureDoneCallback1(webrtc::DesktopFrame* frame);

  // Verifies that a rectangle explicitly marked as dirty is propagated
  // correctly.
  void CaptureDoneCallback2(webrtc::DesktopFrame* frame);

 protected:
  virtual void SetUp() OVERRIDE {
    capturer_ = ScreenCapturer::Create();
  }

  scoped_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerCallback callback_;
};

void ScreenCapturerMacTest::CaptureDoneCallback1(
    webrtc::DesktopFrame* frame) {
  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);

  // Verify that the region contains full frame.
  webrtc::DesktopRegion::Iterator it(frame->updated_region());
  EXPECT_TRUE(!it.IsAtEnd() && it.rect().equals(config.pixel_bounds));
}

void ScreenCapturerMacTest::CaptureDoneCallback2(
    webrtc::DesktopFrame* frame) {
  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);
  int width = config.pixel_bounds.width();
  int height = config.pixel_bounds.height();

  EXPECT_EQ(width, frame->size().width());
  EXPECT_EQ(height, frame->size().height());
  EXPECT_TRUE(frame->data() != NULL);
  // Depending on the capture method, the screen may be flipped or not, so
  // the stride may be positive or negative.
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            abs(frame->stride()));
}

TEST_F(ScreenCapturerMacTest, Capture) {
  EXPECT_CALL(callback_, OnCaptureCompleted(_))
      .Times(2)
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback1))
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback2));

  EXPECT_CALL(callback_, CreateSharedMemory(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(static_cast<webrtc::SharedMemory*>(NULL)));

  SCOPED_TRACE("");
  capturer_->Start(&callback_);

  // Check that we get an initial full-screen updated.
  capturer_->Capture(webrtc::DesktopRegion());

  // Check that subsequent dirty rects are propagated correctly.
  capturer_->Capture(webrtc::DesktopRegion());
}

}  // namespace media
