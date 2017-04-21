// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/CRUILabel+AttributeUtils.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/util/label_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {
class UILabelAttributeUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    _scopedLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    _observer.reset(
        [[LabelObserver observerForLabel:_scopedLabel.get()] retain]);
    [_observer startObserving];
  }

  ~UILabelAttributeUtilsTest() override { [_observer stopObserving]; }

  void CheckLabelLineHeight(CGFloat expected_height) {
    UILabel* label = _scopedLabel.get();
    EXPECT_NE(nil, label.attributedText);
    NSParagraphStyle* style =
        [label.attributedText attribute:NSParagraphStyleAttributeName
                                atIndex:0
                         effectiveRange:nullptr];
    EXPECT_NE(nil, style);
    EXPECT_EQ(expected_height, style.maximumLineHeight);
    EXPECT_EQ(expected_height, label.cr_lineHeight);
  }
  base::scoped_nsobject<UILabel> _scopedLabel;
  base::scoped_nsobject<LabelObserver> _observer;
};
}

TEST_F(UILabelAttributeUtilsTest, TwoObservers) {
  UILabel* label = _scopedLabel.get();
  label.text = @"sample text";

  // Add a second observer.
  base::scoped_nsobject<LabelObserver> secondObserver(
      [[LabelObserver observerForLabel:label] retain]);
  [secondObserver startObserving];

  // Modify the line height with two observers.
  label.cr_lineHeight = 18.0;
  CheckLabelLineHeight(18.0);
  [secondObserver stopObserving];
  secondObserver.reset();

  // Even when one observer is stopped, the second is still observing.
  label.cr_lineHeight = 21.0;
  CheckLabelLineHeight(21.0);

  // Once both are stopped, the height isn't persisted when the text changes.
  [_observer stopObserving];
  label.cr_lineHeight = 25.0;
  label.text = @"longer sample text";
  CheckLabelLineHeight(0);
}

TEST_F(UILabelAttributeUtilsTest, SettingTests) {
  base::scoped_nsobject<UILabel> scopedLabel(
      [[UILabel alloc] initWithFrame:CGRectZero]);
  UILabel* label = _scopedLabel.get();

  // A label should have a line height of 0 if nothing has been specified yet.
  EXPECT_EQ(0.0, label.cr_lineHeight);

  label.text = @"sample text";
  label.cr_lineHeight = 20.0;
  CheckLabelLineHeight(20.0);
  label.text = @"longer sample text";
  CheckLabelLineHeight(20.0);

  base::scoped_nsobject<NSMutableAttributedString> string(
      [[NSMutableAttributedString alloc]
          initWithString:@"attributed sample text"]);
  label.attributedText = string;
  CheckLabelLineHeight(20.0);
  [string addAttribute:NSForegroundColorAttributeName
                 value:[UIColor brownColor]
                 range:NSMakeRange(0, [string length])];
  label.attributedText = string;
  CheckLabelLineHeight(20.0);
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  style.get().maximumLineHeight = 15.0;
  [string addAttribute:NSParagraphStyleAttributeName
                 value:style
                 range:NSMakeRange(0, [string length])];
  CheckLabelLineHeight(20.0);
}

TEST_F(UILabelAttributeUtilsTest, NullTextTest) {
  UILabel* label = _scopedLabel.get();

  label.cr_lineHeight = 19.0;
  EXPECT_EQ(19.0, label.cr_lineHeight);
  label.text = @"sample text";
  CheckLabelLineHeight(19.0);
}

TEST_F(UILabelAttributeUtilsTest, NullAttrTextTest) {
  UILabel* label = _scopedLabel.get();
  base::scoped_nsobject<NSMutableAttributedString> string(
      [[NSMutableAttributedString alloc]
          initWithString:@"attributed sample text"]);
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  style.get().maximumLineHeight = 15.0;
  [string addAttribute:NSParagraphStyleAttributeName
                 value:style
                 range:NSMakeRange(0, [string length])];

  label.cr_lineHeight = 19.0;
  EXPECT_EQ(19.0, label.cr_lineHeight);
  label.attributedText = string;
  CheckLabelLineHeight(19.0);
}
