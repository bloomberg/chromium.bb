// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/javascript_dialog_blocking_util.h"

#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// TestWebState subclass that allows simulating page loads.
class JavaScriptBlockingTestWebState : public web::TestWebState {
 public:
  JavaScriptBlockingTestWebState() : web::TestWebState(), observer_(nullptr) {}
  ~JavaScriptBlockingTestWebState() override {
    if (observer_)
      observer_->WebStateDestroyed();
  }

  // Simulates a page load by sending a WebStateObserver callback.
  void SimulatePageLoaded() {
    web::LoadCommittedDetails details;
    if (observer_)
      observer_->NavigationItemCommitted(details);
  }

 protected:
  // WebState overrides.
  void AddObserver(web::WebStateObserver* observer) override {
    // Currently, only one observer is supported.
    ASSERT_EQ(observer_, nullptr);
    observer_ = observer;
  }

 private:
  web::WebStateObserver* observer_;
};
}  // namespace

// Tests that ShouldShowDialogBlockingOptionForPresenter() returns true
// after the first call to JavaScriptDialogWasShownForPresenter() for a
// given presenter.
TEST(JavaScriptDialogBlockingTest, ShouldShow) {
  JavaScriptBlockingTestWebState webState;
  EXPECT_FALSE(ShouldShowDialogBlockingOption(&webState));
  JavaScriptDialogWasShown(&webState);
  EXPECT_TRUE(ShouldShowDialogBlockingOption(&webState));
}

// Tests that ShouldBlockDialogsForPresenter() returns true after a call
// to DialogBlockingOptionSelectedForPresenter() for a given presenter.
TEST(JavaScriptDialogBlockingTest, BlockingOptionSelected) {
  JavaScriptBlockingTestWebState webState;
  EXPECT_FALSE(ShouldBlockJavaScriptDialogs(&webState));
  DialogBlockingOptionSelected(&webState);
  EXPECT_TRUE(ShouldBlockJavaScriptDialogs(&webState));
}

// Tests that ShouldBlockDialogsForPresenter() returns false after a  page load.
TEST(JavaScriptDialogBlockingTest, StopBlocking) {
  JavaScriptBlockingTestWebState webState;
  EXPECT_FALSE(ShouldBlockJavaScriptDialogs(&webState));
  DialogBlockingOptionSelected(&webState);
  EXPECT_TRUE(ShouldBlockJavaScriptDialogs(&webState));
  webState.SimulatePageLoaded();
  EXPECT_FALSE(ShouldBlockJavaScriptDialogs(&webState));
}
