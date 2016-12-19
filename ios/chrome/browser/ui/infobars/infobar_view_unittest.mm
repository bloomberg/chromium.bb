// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

@interface InfoBarView (Testing)
- (CGFloat)buttonsHeight;
- (CGFloat)buttonMargin;
- (CGFloat)computeRequiredHeightAndLayoutSubviews:(BOOL)layout;
- (CGFloat)heightThatFitsButtonsUnderOtherWidgets:(CGFloat)heightOfFirstLine
                                           layout:(BOOL)layout;
- (CGFloat)minimumInfobarHeight;
- (NSString*)stripMarkersFromString:(NSString*)string;
- (const std::vector<std::pair<NSUInteger, NSRange>>&)linkRanges;
@end

namespace {

const int kShortStringLength = 4;
const int kLongStringLength = 1000;

class InfoBarViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    CGFloat screenWidth = [UIScreen mainScreen].bounds.size.width;
    infobarView_.reset([[InfoBarView alloc]
        initWithFrame:CGRectMake(0, 0, screenWidth, 0)
             delegate:NULL]);
    [infobarView_ addCloseButtonWithTag:0 target:nil action:nil];
  }

  NSString* RandomString(int numberOfCharacters) {
    NSMutableString* string = [NSMutableString string];
    NSString* letters = @"abcde ";
    for (int i = 0; i < numberOfCharacters; i++) {
      [string
          appendFormat:@"%C", [letters characterAtIndex:arc4random_uniform(
                                                            [letters length])]];
    }
    return string;
  }

  NSString* ShortRandomString() { return RandomString(kShortStringLength); }

  NSString* LongRandomString() { return RandomString(kLongStringLength); }

  CGFloat InfobarHeight() {
    return [infobarView_ computeRequiredHeightAndLayoutSubviews:NO];
  }

  CGFloat MinimumInfobarHeight() { return [infobarView_ minimumInfobarHeight]; }

  CGFloat ButtonsHeight() { return [infobarView_ buttonsHeight]; }

  CGFloat ButtonMargin() { return [infobarView_ buttonMargin]; }

  void TestLinkDetectionHelper(
      NSString* input,
      NSString* expectedOutput,
      const std::vector<std::pair<NSUInteger, NSRange>>& expectedRanges) {
    NSString* output = [infobarView_ stripMarkersFromString:input];
    EXPECT_NSEQ(expectedOutput, output);
    const std::vector<std::pair<NSUInteger, NSRange>>& ranges =
        [infobarView_ linkRanges];
    EXPECT_EQ(expectedRanges.size(), ranges.size());
    for (unsigned int i = 0; i < expectedRanges.size(); ++i) {
      EXPECT_EQ(expectedRanges[i].first, ranges[i].first);
      EXPECT_TRUE(NSEqualRanges(expectedRanges[i].second, ranges[i].second));
    }
  }

  base::scoped_nsobject<InfoBarView> infobarView_;
};

TEST_F(InfoBarViewTest, TestLayoutWithNoLabel) {
  // Do not call -addLabel: to test the case when there is no label.
  EXPECT_EQ(MinimumInfobarHeight(), InfobarHeight());
}

TEST_F(InfoBarViewTest, TestLayoutWithShortLabel) {
  [infobarView_ addLabel:ShortRandomString()];
  EXPECT_EQ(MinimumInfobarHeight(), InfobarHeight());
}

TEST_F(InfoBarViewTest, TestLayoutWithLongLabel) {
  [infobarView_ addLabel:LongRandomString()];
  EXPECT_LT(MinimumInfobarHeight(), InfobarHeight());
  EXPECT_EQ(0,
            [infobarView_ heightThatFitsButtonsUnderOtherWidgets:0 layout:NO]);
}

TEST_F(InfoBarViewTest, TestLayoutWithShortButtons) {
  [infobarView_ addLabel:ShortRandomString()];
  [infobarView_ addButton1:ShortRandomString()
                      tag1:0
                   button2:ShortRandomString()
                      tag2:0
                    target:nil
                    action:nil];
  EXPECT_EQ(MinimumInfobarHeight(), InfobarHeight());
  EXPECT_EQ(ButtonsHeight(),
            [infobarView_ heightThatFitsButtonsUnderOtherWidgets:0 layout:NO]);
}

TEST_F(InfoBarViewTest, TestLayoutWithOneLongButtonAndOneShortButton) {
  [infobarView_ addLabel:ShortRandomString()];
  [infobarView_ addButton1:LongRandomString()
                      tag1:0
                   button2:ShortRandomString()
                      tag2:0
                    target:nil
                    action:nil];
  EXPECT_EQ(MinimumInfobarHeight() + ButtonsHeight() * 2 + ButtonMargin(),
            InfobarHeight());
  EXPECT_EQ(ButtonsHeight() * 2,
            [infobarView_ heightThatFitsButtonsUnderOtherWidgets:0 layout:NO]);
}

TEST_F(InfoBarViewTest, TestLayoutWithShortLabelAndShortButton) {
  [infobarView_ addLabel:ShortRandomString()];
  [infobarView_ addButton:ShortRandomString() tag:0 target:nil action:nil];
  EXPECT_EQ(MinimumInfobarHeight(), InfobarHeight());
}

TEST_F(InfoBarViewTest, TestLayoutWithShortLabelAndLongButton) {
  [infobarView_ addLabel:ShortRandomString()];
  [infobarView_ addButton:LongRandomString() tag:0 target:nil action:nil];
  EXPECT_EQ(MinimumInfobarHeight() + ButtonsHeight() + ButtonMargin(),
            InfobarHeight());
}

TEST_F(InfoBarViewTest, TestLayoutWithLongLabelAndLongButtons) {
  [infobarView_ addLabel:LongRandomString()];
  [infobarView_ addButton1:ShortRandomString()
                      tag1:0
                   button2:LongRandomString()
                      tag2:0
                    target:nil
                    action:nil];
  EXPECT_LT(MinimumInfobarHeight() + ButtonsHeight() * 2, InfobarHeight());
}

TEST_F(InfoBarViewTest, TestLinkDetection) {
  [infobarView_ addLabel:ShortRandomString()];
  NSString* linkFoo = [InfoBarView stringAsLink:@"foo" tag:1];
  NSString* linkBar = [InfoBarView stringAsLink:@"bar" tag:2];
  std::vector<std::pair<NSUInteger, NSRange>> ranges;
  // No link.
  TestLinkDetectionHelper(@"", @"", ranges);
  TestLinkDetectionHelper(@"foo", @"foo", ranges);
  // One link.
  ranges.push_back(std::make_pair(1, NSMakeRange(0, 3)));
  TestLinkDetectionHelper(linkFoo, @"foo", ranges);
  NSString* link1 = [NSString stringWithFormat:@"baz%@qux", linkFoo];
  // Link in the middle.
  ranges.clear();
  ranges.push_back(std::make_pair(1, NSMakeRange(3, 3)));
  TestLinkDetectionHelper(link1, @"bazfooqux", ranges);
  // Multiple links.
  NSString* link2 = [NSString stringWithFormat:@"%@%@", linkFoo, linkBar];
  ranges.clear();
  ranges.push_back(std::make_pair(1, NSMakeRange(0, 3)));
  ranges.push_back(std::make_pair(2, NSMakeRange(3, 3)));
  TestLinkDetectionHelper(link2, @"foobar", ranges);
  // Multiple links and text.
  NSString* link3 =
      [NSString stringWithFormat:@"baz%@qux%@tot", linkFoo, linkBar];
  ranges.clear();
  ranges.push_back(std::make_pair(1, NSMakeRange(3, 3)));
  ranges.push_back(std::make_pair(2, NSMakeRange(9, 3)));
  TestLinkDetectionHelper(link3, @"bazfooquxbartot", ranges);
}

}  // namespace
