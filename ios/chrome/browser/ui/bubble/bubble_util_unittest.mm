// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTestBubbleAlignmentOffset = 10.0f;
}  // namespace

class BubbleUtilTest : public PlatformTest {
 public:
  BubbleUtilTest()
      : leftAlignedTarget_(CGRectMake(20.0f, 200.0f, 100.0f, 100.0f)),
        centerAlignedTarget_(CGRectMake(250.0f, 200.0f, 100.0f, 100.0f)),
        rightAlignedTarget_(CGRectMake(400.0f, 200.0f, 100.0f, 100.0f)),
        bubbleSize_(CGSizeMake(300.0f, 100.0f)),
        containerWidth_(500.0f) {}

 protected:
  // Target element on the left side of the container.
  const CGRect leftAlignedTarget_;
  // Target element on the center of the container.
  const CGRect centerAlignedTarget_;
  // Target element on the right side of the container.
  const CGRect rightAlignedTarget_;
  const CGSize bubbleSize_;
  // Bounding width of the bubble's coordinate system.
  const CGFloat containerWidth_;
};

// Test |OriginY| when the bubble points up at the target element.
TEST_F(BubbleUtilTest, OriginUpArrow) {
  CGFloat originY = bubble_util::OriginY(
      centerAlignedTarget_, BubbleArrowDirectionUp, bubbleSize_.height);
  EXPECT_EQ(300.0f, originY);
}

// Test |OriginY| when the bubble points down at the target element.
TEST_F(BubbleUtilTest, OriginDownArrow) {
  CGFloat originY = bubble_util::OriginY(
      centerAlignedTarget_, BubbleArrowDirectionDown, bubbleSize_.height);
  EXPECT_EQ(100.0f, originY);
}

// Test the |LeadingDistance| method when the bubble is leading aligned.
// Positioning the bubble |LeadingDistance| from the leading edge of its
// superview should align the bubble's arrow with the target element.
TEST_F(BubbleUtilTest, LeadingDistanceLeadingAlignment) {
  CGFloat leading = bubble_util::LeadingDistance(
      leftAlignedTarget_, BubbleAlignmentLeading, bubbleSize_.width);
  EXPECT_EQ(70.0f - kTestBubbleAlignmentOffset, leading);
}

// Test the |LeadingDistance| method when the bubble is center aligned.
TEST_F(BubbleUtilTest, LeadingDistanceCenterAlignment) {
  CGFloat leading = bubble_util::LeadingDistance(
      centerAlignedTarget_, BubbleAlignmentCenter, bubbleSize_.width);
  EXPECT_EQ(150.0f, leading);
}

// Test the |LeadingDistance| method when the bubble is trailing aligned.
TEST_F(BubbleUtilTest, LeadingDistanceTrailingAlignment) {
  CGFloat leading = bubble_util::LeadingDistance(
      rightAlignedTarget_, BubbleAlignmentTrailing, bubbleSize_.width);
  EXPECT_EQ(150.0f + kTestBubbleAlignmentOffset, leading);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the left side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnLeft) {
  CGFloat leftAlignedWidth =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(430.0f + kTestBubbleAlignmentOffset, leftAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the center of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnCenter) {
  CGFloat centerAlignedWidth =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(200.0f + kTestBubbleAlignmentOffset, centerAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the right side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnRight) {
  CGFloat rightAlignedWidth =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(50.0f + kTestBubbleAlignmentOffset, rightAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the left side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnLeft) {
  CGFloat leftAlignedWidth =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(140.0f, leftAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the center of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnCenter) {
  CGFloat centerAlignedWidth =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(400.0f, centerAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the right side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnRight) {
  CGFloat rightAlignedWidth =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(100.0f, rightAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the left side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnLeft) {
  CGFloat leftAlignedWidth =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(70.0f + kTestBubbleAlignmentOffset, leftAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the center of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnCenter) {
  CGFloat centerAlignedWidth =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(300.0f + kTestBubbleAlignmentOffset, centerAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the right side of the container, and the language is LTR.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnRight) {
  CGFloat rightAlignedWidth =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, false /* isRTL */);
  EXPECT_EQ(450.0f + kTestBubbleAlignmentOffset, rightAlignedWidth);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the left side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnLeftRTL) {
  CGFloat leftAlignedWidthRTL =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(70.0f + kTestBubbleAlignmentOffset, leftAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the center of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnCenterRTL) {
  CGFloat centerAlignedWidthRTL =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(300.0f + kTestBubbleAlignmentOffset, centerAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is leading aligned, the target is
// on the right side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthLeadingWithTargetOnRightRTL) {
  CGFloat rightAlignedWidthRTL =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentLeading,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(450.0f + kTestBubbleAlignmentOffset, rightAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the left side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnLeftRTL) {
  CGFloat leftAlignedWidthRTL =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(140.0f, leftAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the center of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnCenterRTL) {
  CGFloat centerAlignedWidthRTL =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(400.0f, centerAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is center aligned, the target is
// on the right side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthCenterWithTargetOnRightRTL) {
  CGFloat rightAlignedWidthRTL =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentCenter,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(100.0f, rightAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the left side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnLeftRTL) {
  CGFloat leftAlignedWidthRTL =
      bubble_util::MaxWidth(leftAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(430.0f + kTestBubbleAlignmentOffset, leftAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the center of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnCenterRTL) {
  CGFloat centerAlignedWidthRTL =
      bubble_util::MaxWidth(centerAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(200.0f + kTestBubbleAlignmentOffset, centerAlignedWidthRTL);
}

// Test the |MaxWidth| method when the bubble is trailing aligned, the target is
// on the right side of the container, and the language is RTL.
TEST_F(BubbleUtilTest, MaxWidthTrailingWithTargetOnRightRTL) {
  CGFloat rightAlignedWidthRTL =
      bubble_util::MaxWidth(rightAlignedTarget_, BubbleAlignmentTrailing,
                            containerWidth_, true /* isRTL */);
  EXPECT_EQ(50.0f + kTestBubbleAlignmentOffset, rightAlignedWidthRTL);
}
