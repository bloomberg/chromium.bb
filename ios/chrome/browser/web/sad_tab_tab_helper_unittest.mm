// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Verifies that provided |content_view| exists, contains the expected
// view and matches the desired |mode|.
void VerifyContentViewMatchesMode(CRWContentView* content_view,
                                  SadTabViewMode mode) {
  ASSERT_TRUE([content_view isKindOfClass:[CRWGenericContentView class]]);
  UIView* content_view_view = [(CRWGenericContentView*)content_view view];
  ASSERT_TRUE([content_view_view isKindOfClass:[SadTabView class]]);
  EXPECT_EQ([(SadTabView*)content_view_view mode], mode);
}

class SadTabTabHelperTest : public PlatformTest {
 protected:
  SadTabTabHelperTest() { SadTabTabHelper::CreateForWebState(&web_state_); }
  web::TestWebState web_state_;
};

// TODO(crbug.com/753327): those tests are consistently failing on devices on
// the bots. Remove once they have been fixed.
#if TARGET_OS_SIMULATOR
#define DISABLED_ON_DEVICE(NAME) NAME
#else
#define DISABLED_ON_DEVICE(NAME) DISABLED_##NAME
#endif

// Tests that SadTab is not presented for not shown web states.
TEST_F(SadTabTabHelperTest, DISABLED_ON_DEVICE(NotPresented)) {
  // WebState should not have presented a transient content view.
  EXPECT_FALSE(web_state_.GetTransientContentView());

  // Helper should get notified of render process failure,
  // but Sad Tab should not be presented, because web state was not shown.
  web_state_.OnRenderProcessGone();
  EXPECT_FALSE(web_state_.GetTransientContentView());
}

// Tests that SadTab is presented for shown web states.
TEST_F(SadTabTabHelperTest, DISABLED_ON_DEVICE(Presented)) {
  web_state_.WasShown();

  // WebState should not have presented a transient content view.
  EXPECT_FALSE(web_state_.GetTransientContentView());

  // Helper should get notified of render process failure.
  web_state_.OnRenderProcessGone();
  EXPECT_TRUE(web_state_.GetTransientContentView());
}

// Tests that repeated failures generate the correct UI.
TEST_F(SadTabTabHelperTest, DISABLED_ON_DEVICE(RepeatedFailuresShowCorrectUI)) {
  web_state_.WasShown();

  // Helper should get notified of render process failure.
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
TEST_F(SadTabTabHelperTest, DISABLED_ON_DEVICE(FailureInterval)) {
  // N.B. The test fixture web_state_ is not used for this test as a custom
  // |repeat_failure_interval| is required.
  web::TestWebState web_state;
  SadTabTabHelper::CreateForWebState(&web_state, 0.0f);
  web_state.WasShown();

  // Helper should get notified of render process failure.
  // SadTab should be shown.
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
