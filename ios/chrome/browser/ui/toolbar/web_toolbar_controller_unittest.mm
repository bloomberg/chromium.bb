// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/ios/ios_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/test_toolbar_model_ios.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller_private.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface UIView (SubViewTesting)
- (NSMutableArray*)allSubviews;
@end

@implementation UIView (SubViewTesting)
- (NSMutableArray*)allSubviews {
  NSMutableArray* views = [NSMutableArray array];
  [views addObject:self];
  for (UIView* subview in [self subviews]) {
    [views addObjectsFromArray:[subview allSubviews]];
  }
  return views;
}
@end

@interface WebToolbarController (TestingAdditions)
- (void)layoutOmnibox;
@end

#pragma mark -

namespace {

class WebToolbarControllerTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    // Set some text in the ToolbarModel so that OmniboxView uses the
    // ToolbarModel's icon instead of the OmniboxEditModel's icon.
    toolbar_model_ios_.reset(new TestToolbarModelIOS());
    toolbar_model_ = (TestToolbarModel*)toolbar_model_ios_->GetToolbarModel();
    toolbar_model_->set_text(base::UTF8ToUTF16("foo"));

    // The OCMOCK_VALUE macro doesn't like std::unique_ptr, but it works just
    // fine if a temporary variable is used.
    ToolbarModelIOS* model_for_mock = toolbar_model_ios_.get();
    id delegate =
        [OCMockObject niceMockForProtocol:@protocol(WebToolbarDelegate)];
    [[[delegate stub] andReturnValue:OCMOCK_VALUE(model_for_mock)]
        toolbarModelIOS];

    id urlLoader = [OCMockObject niceMockForProtocol:@protocol(UrlLoader)];

    // Create the WebToolbarController using the test objects.
    web_toolbar_controller_.reset([[WebToolbarController alloc]
        initWithDelegate:delegate
               urlLoader:urlLoader
            browserState:chrome_browser_state_.get()
         preloadProvider:nil]);
    [web_toolbar_controller_ setUnitTesting:YES];
  }
  void TearDown() override {
    web_toolbar_controller_.reset();
    BlockCleanupTest::TearDown();
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TestToolbarModel* toolbar_model_;  // weak. Owned by toolbar_model_ios_.
  std::unique_ptr<TestToolbarModelIOS> toolbar_model_ios_;
  base::scoped_nsobject<WebToolbarController> web_toolbar_controller_;
};

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_NavigationButtonsEnabled) {
  EXPECT_FALSE([web_toolbar_controller_ isForwardButtonEnabled]);
  EXPECT_FALSE([web_toolbar_controller_ isBackButtonEnabled]);
  toolbar_model_ios_->set_can_go_back(true);
  toolbar_model_ios_->set_can_go_forward(true);
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_TRUE([web_toolbar_controller_ isForwardButtonEnabled]);
  EXPECT_TRUE([web_toolbar_controller_ isBackButtonEnabled]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_NavigationButtonsDisabled) {
  EXPECT_FALSE([web_toolbar_controller_ isForwardButtonEnabled]);
  EXPECT_FALSE([web_toolbar_controller_ isBackButtonEnabled]);
  toolbar_model_ios_->set_can_go_back(false);
  toolbar_model_ios_->set_can_go_forward(false);
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_FALSE([web_toolbar_controller_ isForwardButtonEnabled]);
  EXPECT_FALSE([web_toolbar_controller_ isBackButtonEnabled]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_StarButton) {
  // Test tablet only, as star button doesn't get used on phone.
  if (!IsIPadIdiom())
    return;
  EXPECT_FALSE([web_toolbar_controller_ isStarButtonSelected]);
  // Set the curent tab to |kWebUrl| and create a bookmark for |kWebUrl|, then
  // verify that updateToolbar selected the star button.
  toolbar_model_ios_->set_is_current_tab_bookmarked(true);
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_TRUE([web_toolbar_controller_ isStarButtonSelected]);

  // Remove the bookmark and verify updateToolbar unselects the star button.
  toolbar_model_ios_->set_is_current_tab_bookmarked(false);
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_FALSE([web_toolbar_controller_ isStarButtonSelected]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_ProgressBarLoading) {
  // Test phone only, as iPad does not have a progress bar.
  if (IsIPadIdiom())
    return;
  EXPECT_FALSE([web_toolbar_controller_ isLoading]);
  toolbar_model_ios_->set_is_loading(true);
  [web_toolbar_controller_ updateToolbarState];
  // Spin the run loop to allow animation to occur.
  [[NSRunLoop currentRunLoop]
      runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  EXPECT_TRUE([web_toolbar_controller_ isLoading]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_ProgressBarNotLoading) {
  // Test phone only, as iPad does not have a progress bar.
  if (IsIPadIdiom())
    return;
  EXPECT_FALSE([web_toolbar_controller_ isLoading]);
  toolbar_model_ios_->set_is_loading(false);
  [web_toolbar_controller_ updateToolbarState];
  // Spin the run loop to allow animation to occur.
  [[NSRunLoop currentRunLoop]
      runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  EXPECT_FALSE([web_toolbar_controller_ isLoading]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_LoadProgressFraction) {
  // Test phone only, as iPad does not have a progress bar.
  if (IsIPadIdiom())
    return;
  CGFloat expectedProgress = 0.5;
  toolbar_model_ios_->set_is_loading(true);
  toolbar_model_ios_->set_load_progress_fraction(expectedProgress);
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_EQ(expectedProgress, [web_toolbar_controller_ loadProgressFraction]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_PrerenderingAnimation) {
  // Test phone only, as iPad does not have a progress bar.
  if (IsIPadIdiom())
    return;

  EXPECT_FALSE([web_toolbar_controller_ isLoading]);
  EXPECT_FALSE([web_toolbar_controller_ isPrerenderAnimationRunning]);
  [web_toolbar_controller_ showPrerenderingAnimation];
  EXPECT_TRUE([web_toolbar_controller_ isPrerenderAnimationRunning]);
  // |-updateToolbarState| does not cancel the animation.
  [web_toolbar_controller_ updateToolbarState];
  EXPECT_TRUE([web_toolbar_controller_ isPrerenderAnimationRunning]);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_WebUrlText) {
  const char* kExpectedUrlText = "expected URL text";
  toolbar_model_->set_text(base::UTF8ToUTF16(kExpectedUrlText));
  [web_toolbar_controller_ updateToolbarState];
  std::string actualUrlText = [web_toolbar_controller_ getLocationText];
  EXPECT_EQ(kExpectedUrlText, actualUrlText);
}

TEST_F(WebToolbarControllerTest, TestUpdateToolbar_TestLayoutOmnibox) {
  // Test that the initial setup of the toolbar matches the layout produced by
  // calling layoutOmnibox.  If the initial frames do not match those set during
  // layoutOmnibox, there will be minor animations during startup.
  typedef base::hash_map<unsigned long, CGRect> FrameHash;
  typedef base::hash_map<unsigned long, CGRect>::iterator FrameHashIter;
  FrameHash initialFrames;
  FrameHash postLayoutFrames;

  for (UIView* view in [[web_toolbar_controller_ view] allSubviews]) {
    initialFrames.insert(std::make_pair<unsigned long, CGRect>(
        (unsigned long)view, [view frame]));
  }

  [web_toolbar_controller_ layoutOmnibox];

  for (UIView* view in [[web_toolbar_controller_ view] allSubviews]) {
    postLayoutFrames.insert(std::make_pair<unsigned long, CGRect>(
        (unsigned long)view, [view frame]));
  }

  FrameHashIter initialIt = initialFrames.begin();
  while (initialIt != initialFrames.end()) {
    FrameHashIter postLayoutIt = postLayoutFrames.find(initialIt->first);
    EXPECT_TRUE(postLayoutIt != postLayoutFrames.end());
    CGRect initialRect = initialIt->second;
    CGRect layoutRect = postLayoutIt->second;
    EXPECT_EQ(initialRect.origin.x, layoutRect.origin.x);
    EXPECT_EQ(initialRect.origin.y, layoutRect.origin.y);
    EXPECT_EQ(initialRect.size.width, layoutRect.size.width);
    EXPECT_EQ(initialRect.size.height, layoutRect.size.height);
    initialIt++;
  }
}
}  // namespace
