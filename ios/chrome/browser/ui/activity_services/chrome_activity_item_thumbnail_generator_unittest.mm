// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ChromeActivityItemThumbnailGeneratorTest : public PlatformTest {
 protected:
  void SetUp() override {
    tab_ = [OCMockObject niceMockForClass:[Tab class]];

    UIImage* snapshot = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(400, 300), [UIColor redColor]);

    [[[tab_ stub] andReturn:snapshot] generateSnapshotWithOverlay:NO
                                                 visibleFrameOnly:YES];
  }

  void SetBrowserStateOnTab(bool incognito) {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    ios::ChromeBrowserState* browserState =
        incognito ? chrome_browser_state_->GetOffTheRecordChromeBrowserState()
                  : chrome_browser_state_.get();
    [[[tab_ stub] andReturnValue:OCMOCK_VALUE(browserState)] browserState];
  }

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  id tab_;
};

TEST_F(ChromeActivityItemThumbnailGeneratorTest, ThumbnailForNonIncognitoTab) {
  SetBrowserStateOnTab(false);
  CGSize size = CGSizeMake(50, 50);
  ThumbnailGeneratorBlock generatorBlock =
      activity_services::ThumbnailGeneratorForTab(tab_);
  EXPECT_TRUE(generatorBlock);
  UIImage* thumbnail = generatorBlock(size);
  EXPECT_TRUE(thumbnail);
  EXPECT_TRUE(CGSizeEqualToSize(thumbnail.size, size));
}

TEST_F(ChromeActivityItemThumbnailGeneratorTest, NoThumbnailForIncognitoTab) {
  SetBrowserStateOnTab(true);
  CGSize size = CGSizeMake(50, 50);
  ThumbnailGeneratorBlock generatorBlock =
      activity_services::ThumbnailGeneratorForTab(tab_);
  EXPECT_TRUE(generatorBlock);
  UIImage* thumbnail = generatorBlock(size);
  EXPECT_FALSE(thumbnail);
}

}  // namespace
