// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FullscreenWebStateListObserverTest : public PlatformTest {
 public:
  FullscreenWebStateListObserverTest()
      : PlatformTest(),
        web_state_list_(&web_state_list_delegate_),
        observer_(&model_, &web_state_list_) {
    SetUpFullscreenModelForTesting(&model_, 100.0);
  }

  ~FullscreenWebStateListObserverTest() override {
    // Stop observing the WebStateList.
    observer_.Disconnect();
  }

  FullscreenModel& model() { return model_; }
  WebStateList& web_state_list() { return web_state_list_; }

 private:
  FullscreenModel model_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FullscreenWebStateListObserver observer_;
};

TEST_F(FullscreenWebStateListObserverTest, ObserveActiveWebState) {
  // Insert a WebState into the list.  The observer should create a
  // FullscreenWebStateObserver for the newly activated WebState.
  std::unique_ptr<web::TestWebState> inserted_web_state =
      base::MakeUnique<web::TestWebState>();
  web::TestWebState* web_state = inserted_web_state.get();
  web_state_list().InsertWebState(0, std::move(inserted_web_state),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  // Simulate a scroll to 0.5 progress.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(model().progress(), 0.5);
  // Simulate a navigation.  The model should be reset by the observers.
  web::FakeNavigationContext context;
  web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(model().progress(), 1.0);
}
