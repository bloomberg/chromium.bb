// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"

#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// TestWebState subclass that allows simulating page loads.
class JavaScriptBlockingTestWebState : public web::TestWebState {
 public:
  JavaScriptBlockingTestWebState() : web::TestWebState(), observer_(nullptr) {}
  ~JavaScriptBlockingTestWebState() override {
    observer_->WebStateDestroyed(this);
  }

  // Simulates committing a navigation by sending a WebStateObserver callback.
  void SimulateNavigationCommitted() {
    web::LoadCommittedDetails details;
    observer_->NavigationItemCommitted(this, details);
  }

 protected:
  // WebState:
  void AddObserver(web::WebStateObserver* observer) override {
    // Only one observer is supported.
    ASSERT_EQ(observer_, nullptr);
    observer_ = observer;
  }

 private:
  web::WebStateObserver* observer_;
};
}  // namespace

// Test fixture for testing JavaScriptDialogBlockingState.
class JavaScriptDialogBlockingStateTest : public PlatformTest {
 protected:
  JavaScriptDialogBlockingStateTest() : PlatformTest() {
    JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  }

  JavaScriptDialogBlockingState& state() {
    return *JavaScriptDialogBlockingState::FromWebState(&web_state_);
  }
  JavaScriptBlockingTestWebState& web_state() { return web_state_; }

 private:
  JavaScriptBlockingTestWebState web_state_;
};

// Tests that show_blocking_option() returns true after the first call to
// JavaScriptDialogWasShown() for a given presenter.
TEST_F(JavaScriptDialogBlockingStateTest, ShouldShow) {
  EXPECT_FALSE(state().show_blocking_option());
  state().JavaScriptDialogWasShown();
  EXPECT_TRUE(state().show_blocking_option());
}

// Tests that blocked() returns true after a call
// to JavaScriptDialogBlockingOptionSelected() for a given presenter.
TEST_F(JavaScriptDialogBlockingStateTest, BlockingOptionSelected) {
  EXPECT_FALSE(state().blocked());
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
}

// Tests that blocked() returns false after a  page load.
TEST_F(JavaScriptDialogBlockingStateTest, StopBlocking) {
  EXPECT_FALSE(state().blocked());
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationCommitted();
  EXPECT_FALSE(state().blocked());
}
