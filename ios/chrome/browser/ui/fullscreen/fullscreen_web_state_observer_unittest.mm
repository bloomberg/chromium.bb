// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FullscreenWebStateObserverTest : public PlatformTest {
 public:
  FullscreenWebStateObserverTest() : PlatformTest(), observer_(&model_) {
    // Set up model.
    SetUpFullscreenModelForTesting(&model_, 100.0);
    // Set up a TestNavigationManager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        base::MakeUnique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    // Begin observing the WebState.
    observer_.SetWebState(&web_state_);
  }

  ~FullscreenWebStateObserverTest() override {
    // Stop observing the WebState.
    observer_.SetWebState(nullptr);
  }

  FullscreenModel& model() { return model_; }
  web::TestWebState& web_state() { return web_state_; }
  web::TestNavigationManager& navigation_manager() {
    return *navigation_manager_;
  }

 private:
  FullscreenModel model_;
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_;
  FullscreenWebStateObserver observer_;
};

// Tests that the model is reset when a navigation is committed.
TEST_F(FullscreenWebStateObserverTest, ResetForNavigation) {
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(model().progress(), 0.5);
  // Simulate a navigation.
  web::FakeNavigationContext context;
  web_state().OnNavigationFinished(&context);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(model().progress(), 1.0);
}

// Tests that the model is disabled when a load is occurring.
TEST_F(FullscreenWebStateObserverTest, DisableDuringLoad) {
  EXPECT_TRUE(model().enabled());
  web_state().SetLoading(true);
  EXPECT_FALSE(model().enabled());
  web_state().SetLoading(false);
  EXPECT_TRUE(model().enabled());
}

// Tests that the model is disabled when the SSL status is broken.
TEST_F(FullscreenWebStateObserverTest, DisableForBrokenSSL) {
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->GetSSL().security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  navigation_manager().SetLastCommittedItem(item.get());
  EXPECT_TRUE(model().enabled());
  web_state().OnVisibleSecurityStateChanged();
  EXPECT_FALSE(model().enabled());
  navigation_manager().SetLastCommittedItem(nullptr);
  web_state().OnVisibleSecurityStateChanged();
  EXPECT_TRUE(model().enabled());
}
