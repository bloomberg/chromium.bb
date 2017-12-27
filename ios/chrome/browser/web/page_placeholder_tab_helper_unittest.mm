// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/test/scoped_task_environment.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A configurable TabHelper delegate for testing.
@interface PagePlaceholderTabHelperTestDelegate
    : NSObject<PagePlaceholderTabHelperDelegate>

// YES if Page Placeholder is currently displayed.
@property(nonatomic, readonly, getter=isDisplayingPlaceholder)
    BOOL displayingPlaceholder;

// Tab helper which delegates to this class.
@property(nonatomic, assign) PagePlaceholderTabHelper* tabHelper;

@end

@implementation PagePlaceholderTabHelperTestDelegate

@synthesize displayingPlaceholder = _displayingPlaceholder;
@synthesize tabHelper = _tabHelper;

- (void)displayPlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper {
  EXPECT_EQ(_tabHelper, tabHelper);
  EXPECT_FALSE(_displayingPlaceholder);
  _displayingPlaceholder = YES;
}

- (void)removePlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper {
  EXPECT_EQ(_tabHelper, tabHelper);
  EXPECT_TRUE(_displayingPlaceholder);
  _displayingPlaceholder = NO;
}

@end

// Test fixture for PagePlaceholderTabHelper class.
class PagePlaceholderTabHelperTest : public PlatformTest {
 protected:
  PagePlaceholderTabHelperTest()
      : delegate_([[PagePlaceholderTabHelperTestDelegate alloc] init]),
        web_state_(std::make_unique<web::TestWebState>()) {
    PagePlaceholderTabHelper::CreateForWebState(web_state_.get(), delegate_);
    delegate_.tabHelper = tab_helper();
  }

  PagePlaceholderTabHelper* tab_helper() {
    return PagePlaceholderTabHelper::FromWebState(web_state_.get());
  }

  base::test::ScopedTaskEnvironment environement_;
  PagePlaceholderTabHelperTestDelegate* delegate_;
  std::unique_ptr<web::TestWebState> web_state_;
};

// Tests that placeholder is not shown after DidStartNavigation if it was not
// requested.
TEST_F(PagePlaceholderTabHelperTest, NotShown) {
  ASSERT_FALSE(delegate_.displayingPlaceholder);
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_FALSE(delegate_.displayingPlaceholder);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placehold is not shown after DidStartNavigation if it was
// cancelled before the navigation.
TEST_F(PagePlaceholderTabHelperTest, NotShownIfCancelled) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(delegate_.displayingPlaceholder);
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->CancelPlaceholderForNextNavigation();
  ASSERT_FALSE(delegate_.displayingPlaceholder);
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_FALSE(delegate_.displayingPlaceholder);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is shown between DidStartNavigation/PageLoaded
// WebStateObserver callbacks.
TEST_F(PagePlaceholderTabHelperTest, Shown) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(delegate_.displayingPlaceholder);
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_TRUE(delegate_.displayingPlaceholder);
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(delegate_.displayingPlaceholder);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that placeholder is removed if cancelled while presented.
TEST_F(PagePlaceholderTabHelperTest, RemovedIfCancelledWhileShown) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(delegate_.displayingPlaceholder);
  ASSERT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_TRUE(delegate_.displayingPlaceholder);
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());

  tab_helper()->CancelPlaceholderForNextNavigation();
  EXPECT_FALSE(delegate_.displayingPlaceholder);
  EXPECT_FALSE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
}

// Tests that destructing WebState removes the placeholder.
TEST_F(PagePlaceholderTabHelperTest, DestructWebStateWhenShowingPlaceholder) {
  ASSERT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());
  tab_helper()->AddPlaceholderForNextNavigation();
  ASSERT_FALSE(delegate_.displayingPlaceholder);

  EXPECT_TRUE(tab_helper()->will_add_placeholder_for_next_navigation());
  web_state_->OnNavigationStarted(nullptr);
  EXPECT_TRUE(delegate_.displayingPlaceholder);
  EXPECT_TRUE(tab_helper()->displaying_placeholder());
  EXPECT_FALSE(tab_helper()->will_add_placeholder_for_next_navigation());

  web_state_.reset();
  EXPECT_FALSE(delegate_.displayingPlaceholder);
  // The tab helper has been deleted at this point, so do not check the value
  // of displaying_placeholder().
}
