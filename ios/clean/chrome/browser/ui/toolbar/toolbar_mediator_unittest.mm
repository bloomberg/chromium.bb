// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/shared/chrome/browser/ui/toolbar/toolbar_test_util.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestToolbarMediator : ToolbarMediator<CRWWebStateObserver>
@end

@implementation TestToolbarMediator
@end

namespace {

static const char kTestUrl[] = "http://www.chromium.org";

class ToolbarMediatorTest : public PlatformTest {
 public:
  ToolbarMediatorTest() {
    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        base::MakeUnique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    test_web_state_.SetNavigationManager(std::move(navigation_manager));
    test_web_state_.SetLoading(true);
    mediator_ = [[TestToolbarMediator alloc] init];
    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
  }

 protected:
  TestToolbarMediator* mediator_;
  ToolbarTestWebState test_web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  id consumer_;
};

// Test no setup is being done on the Toolbar if there's no Webstate.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoWebstate) {
  mediator_.consumer = consumer_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setIsLoading:YES];
}

// Test no setup is being done on the Toolbar if there's no consumer.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoConsumer) {
  mediator_.webState = &test_web_state_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setIsLoading:YES];
}

// Test the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set.
TEST_F(ToolbarMediatorTest, TestToolbarSetup) {
  mediator_.webState = &test_web_state_;
  mediator_.consumer = consumer_;

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setIsLoading:YES];
}

// Test the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set in reverse order.
TEST_F(ToolbarMediatorTest, TestToolbarSetupReverse) {
  mediator_.consumer = consumer_;
  mediator_.webState = &test_web_state_;

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setIsLoading:YES];
}

// Test the Toolbar is updated when the Webstate observer method DidStartLoading
// is triggered by SetLoading.
TEST_F(ToolbarMediatorTest, TestDidStartLoading) {
  // Change the default loading state to false so we can verify the Webstate
  // callback with true.
  test_web_state_.SetLoading(false);
  mediator_.webState = &test_web_state_;
  mediator_.consumer = consumer_;

  test_web_state_.SetLoading(true);
  [[consumer_ verify] setIsLoading:YES];
}

// Test the Toolbar is updated when the Webstate observer method DidStopLoading
// is triggered by SetLoading.
TEST_F(ToolbarMediatorTest, TestDidStopLoading) {
  mediator_.webState = &test_web_state_;
  mediator_.consumer = consumer_;

  test_web_state_.SetLoading(false);
  [[consumer_ verify] setIsLoading:NO];
}

// Test the Toolbar is updated when the Webstate observer method
// DidLoadPageWithSuccess is triggered by OnPageLoaded.
TEST_F(ToolbarMediatorTest, TestDidLoadPageWithSucess) {
  mediator_.webState = &test_web_state_;
  mediator_.consumer = consumer_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  test_web_state_.SetCurrentURL(GURL(kTestUrl));
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
}

// Test the Toolbar is updated when the Webstate observer method
// didChangeLoadingProgress is called.
TEST_F(ToolbarMediatorTest, TestLoadingProgress) {
  mediator_.webState = &test_web_state_;
  mediator_.consumer = consumer_;

  [mediator_ webState:mediator_.webState didChangeLoadingProgress:0.42];
  [[consumer_ verify] setLoadingProgress:0.42];
}

}  // namespace
