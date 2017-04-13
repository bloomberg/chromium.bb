// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A configurable TabHelper delegate for testing.
@interface TabHelperTestDelegate : NSObject<SadTabTabHelperDelegate>
@property(readwrite, assign) BOOL active;
@end

@implementation TabHelperTestDelegate
@synthesize active = _active;
- (BOOL)isTabVisibleForTabHelper:(SadTabTabHelper*)tabHelper {
  return self.active;
}
@end

class SadTabTabHelperTest : public PlatformTest {
 protected:
  SadTabTabHelperTest() : delegate_([[TabHelperTestDelegate alloc] init]) {
    SadTabTabHelper::CreateForWebState(&web_state_, delegate_);
  }
  TabHelperTestDelegate* delegate_;
  web::TestWebState web_state_;
};

// Tests that the presentation-block can be suppressed by the delegate.
TEST_F(SadTabTabHelperTest, PresentationCanBeSuppressedByDelegate) {
  // WebState should not have presented a transient content view.
  EXPECT_FALSE(web_state_.IsShowingTransientContentView());

  // Helper should get notified of render process failure,
  // but the delegate should suppress the presentation of a content view.
  web_state_.OnRenderProcessGone();
  EXPECT_FALSE(web_state_.IsShowingTransientContentView());
}

// Tests that the presentation-block can be allowed by the delegate.
TEST_F(SadTabTabHelperTest, PresentationCanBeAllowedByDelegate) {
  delegate_.active = YES;

  // WebState should not have presented a transient content view.
  EXPECT_FALSE(web_state_.IsShowingTransientContentView());

  // Helper should get notified of render process failure.
  // The delegate should allow the presentation of a content view.
  web_state_.OnRenderProcessGone();
  EXPECT_TRUE(web_state_.IsShowingTransientContentView());
}
