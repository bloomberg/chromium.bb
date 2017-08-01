// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestToolbarMediator
    : ToolbarMediator<CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestToolbarMediator
@end

namespace {

static const int kNumberOfWebStates = 3;
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

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~ToolbarMediatorTest() override { [mediator_ disconnect]; }

 protected:
  void SetUpWebStateList() {
    web_state_list_ = base::MakeUnique<WebStateList>(&web_state_list_delegate_);
    for (int i = 0; i < kNumberOfWebStates; i++) {
      InsertWebState(i);
    }
  }

  void InsertWebState(int index) {
    auto web_state = base::MakeUnique<web::TestWebState>();
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state));
  }

  TestToolbarMediator* mediator_;
  ToolbarTestWebState test_web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
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

// Test no WebstateList related setup is being done on the Toolbar if there's no
// WebstateList.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoWebstateList) {
  mediator_.consumer = consumer_;
  mediator_.webState = &test_web_state_;

  [[[consumer_ reject] ignoringNonObjectArgs] setTabCount:0];
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

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set.
TEST_F(ToolbarMediatorTest, TestWebstateListRelatedSetup) {
  SetUpWebStateList();
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  [[consumer_ verify] setTabCount:3];
}

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set in reverse order.
TEST_F(ToolbarMediatorTest, TestWebstateListRelatedSetupReverse) {
  mediator_.consumer = consumer_;
  SetUpWebStateList();
  mediator_.webStateList = web_state_list_.get();

  [[consumer_ verify] setTabCount:3];
}

// Test the Toolbar is updated when the Webstate observer method DidStartLoading
// is triggered by SetLoading.
TEST_F(ToolbarMediatorTest, TestDidStartLoading) {
  // Change the default loading state to false to verify the Webstate
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
  [[consumer_ verify] setLoadingProgressFraction:0.42];
}

// Test that the mediator's observation of -broadcastTabStripVisible: triggers
// the correct consumer calls.
TEST_F(ToolbarMediatorTest, TestTabStripVisible) {
  mediator_.consumer = consumer_;

  [mediator_ broadcastTabStripVisible:YES];
  [[consumer_ verify] setTabStripVisible:YES];
  [mediator_ broadcastTabStripVisible:NO];
  [[consumer_ verify] setTabStripVisible:NO];
}

// Test that increasing the number of Webstates will update the consumer with
// the right value.
TEST_F(ToolbarMediatorTest, TestIncreaseNumberOfWebstates) {
  SetUpWebStateList();
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  InsertWebState(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates + 1];
}

// Test that decreasing the number of Webstates will update the consumer with
// the right value.
TEST_F(ToolbarMediatorTest, TestDecreaseNumberOfWebstates) {
  SetUpWebStateList();
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  web_state_list_->DetachWebStateAt(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates - 1];
}

}  // namespace
