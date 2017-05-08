// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A configurable TabHelper delegate for testing.
@interface TabHelperTestDelegate : NSObject<SadTabTabHelperDelegate>
// Stores the internal value as to whether the delegate is behaving as though
// it is active or inactive.
@property(readwrite, assign) BOOL active;
@end

@implementation TabHelperTestDelegate
@synthesize active = _active;
- (BOOL)isTabVisibleForTabHelper:(SadTabTabHelper*)tabHelper {
  return self.active;
}
@end

// Verifies that provided |content_view| exists, contains the expected
// view and matches the desired |mode|.
void VerifyContentViewMatchesMode(CRWContentView* content_view,
                                  SadTabViewMode mode) {
  EXPECT_TRUE([content_view isKindOfClass:[CRWGenericContentView class]]);
  UIView* content_view_view = [(CRWGenericContentView*)content_view view];
  EXPECT_TRUE([content_view_view isKindOfClass:[SadTabView class]]);
  EXPECT_EQ([(SadTabView*)content_view_view mode], mode);
}

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
  EXPECT_FALSE(web_state_.GetTransientContentView());

  // Helper should get notified of render process failure,
  // but the delegate should suppress the presentation of a content view.
  web_state_.OnRenderProcessGone();
  EXPECT_FALSE(web_state_.GetTransientContentView());
}

// Tests that the presentation-block can be allowed by the delegate.
TEST_F(SadTabTabHelperTest, PresentationCanBeAllowedByDelegate) {
  delegate_.active = YES;

  // WebState should not have presented a transient content view.
  EXPECT_FALSE(web_state_.GetTransientContentView());

  // Helper should get notified of render process failure.
  // The delegate should allow the presentation of a content view.
  web_state_.OnRenderProcessGone();
  EXPECT_TRUE(web_state_.GetTransientContentView());
}

// Tests that repeated failures generate the correct UI.
TEST_F(SadTabTabHelperTest, RepeatedFailuresShowCorrectUI) {
  delegate_.active = YES;

  // Helper should get notified of render process failure.
  // The delegate should allow the presentation of a content view.
  web_state_.OnRenderProcessGone();

  // The content view should initially be of the RELOAD type.
  VerifyContentViewMatchesMode(web_state_.GetTransientContentView(),
                               SadTabViewMode::RELOAD);

  // On a second render process crash, the content view should be of a
  // FEEDBACK type.
  web_state_.OnRenderProcessGone();
  VerifyContentViewMatchesMode(web_state_.GetTransientContentView(),
                               SadTabViewMode::FEEDBACK);

  // All subsequent crashes should be of a FEEDBACK type.
  web_state_.OnRenderProcessGone();
  VerifyContentViewMatchesMode(web_state_.GetTransientContentView(),
                               SadTabViewMode::FEEDBACK);
}

// Tests that repeated failures can time out, and return to the RELOAD UI.
TEST_F(SadTabTabHelperTest, FailureInterval) {
  // Delegate should respond to failure interval selectors, and have an
  // immediate timeout to test the reset mechanism.

  // N.B. The test fixture web_state_ is not used for this test as a custom
  // |repeat_failure_interval| is required.
  TabHelperTestDelegate* delegate = [[TabHelperTestDelegate alloc] init];
  delegate.active = YES;
  web::TestWebState web_state;
  SadTabTabHelper::CreateForWebState(&web_state, delegate, 0.0f);

  // Helper should get notified of render process failure.
  // The delegate should allow the presentation of a content view.
  web_state.OnRenderProcessGone();

  // The content view should initially be of the RELOAD type.
  VerifyContentViewMatchesMode(web_state.GetTransientContentView(),
                               SadTabViewMode::RELOAD);

  // On a second render process crash, the content view should still be of a
  // RELOAD type due to the 0.0f interval timeout.
  web_state.OnRenderProcessGone();
  VerifyContentViewMatchesMode(web_state.GetTransientContentView(),
                               SadTabViewMode::RELOAD);
}
