// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include <ApplicationServices/ApplicationServices.h>

#include <ostream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "media/video/capture/screen/screen_capturer_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace media {

class ScreenCapturerMacTest : public testing::Test {
 public:
  // Verifies that the whole screen is initially dirty.
  void CaptureDoneCallback1(scoped_refptr<ScreenCaptureData> capture_data);

  // Verifies that a rectangle explicitly marked as dirty is propagated
  // correctly.
  void CaptureDoneCallback2(scoped_refptr<ScreenCaptureData> capture_data);

 protected:
  virtual void SetUp() OVERRIDE {
    capturer_ = ScreenCapturer::Create();
  }

  void AddDirtyRect() {
    SkIRect rect = SkIRect::MakeXYWH(0, 0, 10, 10);
    region_.op(rect, SkRegion::kUnion_Op);
  }

  scoped_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerDelegate delegate_;
  SkRegion region_;
};

void ScreenCapturerMacTest::CaptureDoneCallback1(
    scoped_refptr<ScreenCaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);
  SkRegion initial_region(SkIRect::MakeXYWH(0, 0, width, height));
  EXPECT_EQ(initial_region, capture_data->dirty_region());
}

void ScreenCapturerMacTest::CaptureDoneCallback2(
    scoped_refptr<ScreenCaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);

  EXPECT_EQ(region_, capture_data->dirty_region());
  EXPECT_EQ(width, capture_data->size().width());
  EXPECT_EQ(height, capture_data->size().height());
  EXPECT_TRUE(capture_data->data() != NULL);
  // Depending on the capture method, the screen may be flipped or not, so
  // the stride may be positive or negative.
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            abs(capture_data->stride()));
}

TEST_F(ScreenCapturerMacTest, Capture) {
  EXPECT_CALL(delegate_, OnCaptureCompleted(_))
      .Times(2)
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback1))
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback2));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(delegate_, CreateSharedBuffer(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(scoped_refptr<SharedBuffer>()));

  SCOPED_TRACE("");
  capturer_->Start(&delegate_);

  // Check that we get an initial full-screen updated.
  capturer_->CaptureFrame();

  // Check that subsequent dirty rects are propagated correctly.
  AddDirtyRect();
  capturer_->InvalidateRegion(region_);
  capturer_->CaptureFrame();
  capturer_->Stop();
}

}  // namespace media

namespace gfx {

std::ostream& operator<<(std::ostream& out, const SkRegion& region) {
  out << "SkRegion(";
  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    const SkIRect& r = i.rect();
    out << "(" << r.fLeft << ","  << r.fTop << ","
        << r.fRight  << ","  << r.fBottom << ")";
  }
  out << ")";
  return out;
}

}  // namespace gfx
