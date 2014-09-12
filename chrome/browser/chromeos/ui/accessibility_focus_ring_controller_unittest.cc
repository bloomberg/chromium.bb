// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TestableAccessibilityFocusRingController
    : public AccessibilityFocusRingController {
 public:
  TestableAccessibilityFocusRingController() {}
  virtual ~TestableAccessibilityFocusRingController() {}

  void RectsToRings(const std::vector<gfx::Rect>& rects,
                    std::vector<AccessibilityFocusRing>* rings) const {
    AccessibilityFocusRingController::RectsToRings(rects, rings);
  }

  // Return an easy round number for testing.
  virtual int GetMargin() const OVERRIDE {
    return 10;
  }
};

class AccessibilityFocusRingControllerTest : public testing::Test {
 public:
  AccessibilityFocusRingControllerTest() {}
  virtual ~AccessibilityFocusRingControllerTest() {}

 protected:
  TestableAccessibilityFocusRingController controller_;
};

TEST_F(AccessibilityFocusRingControllerTest, RectsToRingsSimpleBoundsCheck) {
  // Easy sanity check. Given a single rectangle, make sure we get back
  // a focus ring with the same bounds.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 30, 70, 150));
  std::vector<AccessibilityFocusRing> rings;
  controller_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(rects[0], rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingControllerTest, RectsToRingsVerticalStack) {
  // Given two rects, one on top of each other, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 10, 60, 30));
  rects.push_back(gfx::Rect(10, 40, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  controller_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(gfx::Rect(10, 10, 60, 60), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingControllerTest, RectsToRingsHorizontalStack) {
  // Given two rects, one next to the other horizontally, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 10, 60, 30));
  rects.push_back(gfx::Rect(70, 10, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  controller_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(gfx::Rect(10, 10, 120, 30), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingControllerTest, RectsToRingsParagraphShape) {
  // Given a simple paragraph shape, make sure we get something that
  // outlines it correctly.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 200, 100));
  rects.push_back(gfx::Rect(0, 100, 600, 300));
  rects.push_back(gfx::Rect(400, 400, 200, 100));
  std::vector<AccessibilityFocusRing> rings;
  controller_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(gfx::Rect(0, 0, 600, 500), rings[0].GetBounds());

  const gfx::Point* points = rings[0].points;
  ASSERT_EQ(gfx::Point(0, 90), points[0]);
  ASSERT_EQ(gfx::Point(0, 10), points[1]);
  ASSERT_EQ(gfx::Point(0, 0), points[2]);
  ASSERT_EQ(gfx::Point(10, 0), points[3]);
  ASSERT_EQ(gfx::Point(190, 0), points[4]);
  ASSERT_EQ(gfx::Point(200, 0), points[5]);
  ASSERT_EQ(gfx::Point(200, 10), points[6]);
  ASSERT_EQ(gfx::Point(200, 90), points[7]);
  ASSERT_EQ(gfx::Point(200, 100), points[8]);
  ASSERT_EQ(gfx::Point(210, 100), points[9]);
  ASSERT_EQ(gfx::Point(590, 100), points[10]);
  ASSERT_EQ(gfx::Point(600, 100), points[11]);
  ASSERT_EQ(gfx::Point(600, 110), points[12]);
  ASSERT_EQ(gfx::Point(600, 390), points[13]);
  ASSERT_EQ(gfx::Point(600, 400), points[14]);
  ASSERT_EQ(gfx::Point(600, 400), points[15]);
  ASSERT_EQ(gfx::Point(600, 400), points[16]);
  ASSERT_EQ(gfx::Point(600, 400), points[17]);
  ASSERT_EQ(gfx::Point(600, 410), points[18]);
  ASSERT_EQ(gfx::Point(600, 490), points[19]);
  ASSERT_EQ(gfx::Point(600, 500), points[20]);
  ASSERT_EQ(gfx::Point(590, 500), points[21]);
  ASSERT_EQ(gfx::Point(410, 500), points[22]);
  ASSERT_EQ(gfx::Point(400, 500), points[23]);
  ASSERT_EQ(gfx::Point(400, 490), points[24]);
  ASSERT_EQ(gfx::Point(400, 410), points[25]);
  ASSERT_EQ(gfx::Point(400, 400), points[26]);
  ASSERT_EQ(gfx::Point(390, 400), points[27]);
  ASSERT_EQ(gfx::Point(10, 400), points[28]);
  ASSERT_EQ(gfx::Point(0, 400), points[29]);
  ASSERT_EQ(gfx::Point(0, 390), points[30]);
  ASSERT_EQ(gfx::Point(0, 110), points[31]);
  ASSERT_EQ(gfx::Point(0, 100), points[32]);
  ASSERT_EQ(gfx::Point(0, 100), points[33]);
  ASSERT_EQ(gfx::Point(0, 100), points[34]);
  ASSERT_EQ(gfx::Point(0, 100), points[35]);
}

}  // namespace chromeos
