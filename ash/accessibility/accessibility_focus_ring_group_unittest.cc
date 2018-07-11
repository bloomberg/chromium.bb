// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_group.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_focus_ring.h"
#include "ash/accessibility/accessibility_focus_ring_layer.h"
#include "ash/accessibility/accessibility_layer.h"
#include "ash/public/interfaces/accessibility_focus_ring_controller.mojom.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class TestableAccessibilityFocusRingGroup : public AccessibilityFocusRingGroup {
 public:
  TestableAccessibilityFocusRingGroup() : AccessibilityFocusRingGroup() {
    // By default use an easy round number for testing.
    margin_ = 10;
  }
  ~TestableAccessibilityFocusRingGroup() override = default;

  void RectsToRings(const std::vector<gfx::Rect>& rects,
                    std::vector<AccessibilityFocusRing>* rings) const {
    AccessibilityFocusRingGroup::RectsToRings(rects, rings);
  }

  int GetMargin() const override { return margin_; }

 private:
  int margin_;
};

class AccessibilityFocusRingGroupTest : public AshTestBase {
 public:
  AccessibilityFocusRingGroupTest() = default;
  ~AccessibilityFocusRingGroupTest() override = default;

 protected:
  gfx::Rect AddMargin(gfx::Rect r) {
    r.Inset(-group_.GetMargin(), -group_.GetMargin());
    return r;
  }

  TestableAccessibilityFocusRingGroup group_;
};

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsSimpleBoundsCheck) {
  // Easy sanity check. Given a single rectangle, make sure we get back
  // a focus ring with the same bounds.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 30, 70, 150));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(rects[0]), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsVerticalStack) {
  // Given two rects, one on top of each other, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 10, 60, 30));
  rects.push_back(gfx::Rect(10, 40, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(gfx::Rect(10, 10, 60, 60)), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsHorizontalStack) {
  // Given two rects, one next to the other horizontally, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 10, 60, 30));
  rects.push_back(gfx::Rect(70, 10, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(gfx::Rect(10, 10, 120, 30)), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsParagraphShape) {
  // Given a simple paragraph shape, make sure we get something that
  // outlines it correctly.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 10, 180, 80));
  rects.push_back(gfx::Rect(10, 110, 580, 280));
  rects.push_back(gfx::Rect(410, 410, 180, 80));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), rings[0].GetBounds());

  const gfx::Point* points = rings[0].points;
  EXPECT_EQ(gfx::Point(0, 90), points[0]);
  EXPECT_EQ(gfx::Point(0, 10), points[1]);
  EXPECT_EQ(gfx::Point(0, 0), points[2]);
  EXPECT_EQ(gfx::Point(10, 0), points[3]);
  EXPECT_EQ(gfx::Point(190, 0), points[4]);
  EXPECT_EQ(gfx::Point(200, 0), points[5]);
  EXPECT_EQ(gfx::Point(200, 10), points[6]);
  EXPECT_EQ(gfx::Point(200, 90), points[7]);
  EXPECT_EQ(gfx::Point(200, 100), points[8]);
  EXPECT_EQ(gfx::Point(210, 100), points[9]);
  EXPECT_EQ(gfx::Point(590, 100), points[10]);
  EXPECT_EQ(gfx::Point(600, 100), points[11]);
  EXPECT_EQ(gfx::Point(600, 110), points[12]);
  EXPECT_EQ(gfx::Point(600, 390), points[13]);
  EXPECT_EQ(gfx::Point(600, 400), points[14]);
  EXPECT_EQ(gfx::Point(600, 400), points[15]);
  EXPECT_EQ(gfx::Point(600, 400), points[16]);
  EXPECT_EQ(gfx::Point(600, 400), points[17]);
  EXPECT_EQ(gfx::Point(600, 410), points[18]);
  EXPECT_EQ(gfx::Point(600, 490), points[19]);
  EXPECT_EQ(gfx::Point(600, 500), points[20]);
  EXPECT_EQ(gfx::Point(590, 500), points[21]);
  EXPECT_EQ(gfx::Point(410, 500), points[22]);
  EXPECT_EQ(gfx::Point(400, 500), points[23]);
  EXPECT_EQ(gfx::Point(400, 490), points[24]);
  EXPECT_EQ(gfx::Point(400, 410), points[25]);
  EXPECT_EQ(gfx::Point(400, 400), points[26]);
  EXPECT_EQ(gfx::Point(390, 400), points[27]);
  EXPECT_EQ(gfx::Point(10, 400), points[28]);
  EXPECT_EQ(gfx::Point(0, 400), points[29]);
  EXPECT_EQ(gfx::Point(0, 390), points[30]);
  EXPECT_EQ(gfx::Point(0, 110), points[31]);
  EXPECT_EQ(gfx::Point(0, 100), points[32]);
  EXPECT_EQ(gfx::Point(0, 100), points[33]);
  EXPECT_EQ(gfx::Point(0, 100), points[34]);
  EXPECT_EQ(gfx::Point(0, 100), points[35]);
}

}  // namespace ash