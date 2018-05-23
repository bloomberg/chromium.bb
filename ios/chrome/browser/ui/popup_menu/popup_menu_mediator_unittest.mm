// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"
#include "base/time/default_clock.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kNumberOfWebStates = 3;
}  // namespace

@interface TestPopupMenuMediator
    : PopupMenuMediator<CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestPopupMenuMediator
@end

class PopupMenuMediatorTest : public PlatformTest {
 public:
  PopupMenuMediatorTest() {
    reading_list_model_.reset(new ReadingListModelImpl(
        nullptr, nullptr, base::DefaultClock::GetInstance()));
    mediator_incognito_ =
        [[PopupMenuMediator alloc] initWithType:PopupMenuTypeToolsMenu
                                    isIncognito:YES
                               readingListModel:reading_list_model_.get()];
    mediator_ =
        [[PopupMenuMediator alloc] initWithType:PopupMenuTypeToolsMenu
                                    isIncognito:NO
                               readingListModel:reading_list_model_.get()];
    popup_menu_ = OCMClassMock([PopupMenuTableViewController class]);
    popup_menu_strict_ =
        OCMStrictClassMock([PopupMenuTableViewController class]);
    OCMExpect([popup_menu_strict_ setPopupMenuItems:[OCMArg any]]);
    OCMExpect([popup_menu_strict_ setCommandHandler:[OCMArg any]]);
    SetUpWebStateList();
  }

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~PopupMenuMediatorTest() override {
    [mediator_incognito_ disconnect];
    [mediator_ disconnect];
  }

 protected:
  void SetUpWebStateList() {
    auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();

    std::unique_ptr<ToolbarTestWebState> test_web_state =
        std::make_unique<ToolbarTestWebState>();
    test_web_state->SetNavigationManager(std::move(navigation_manager));
    test_web_state->SetLoading(true);
    web_state_ = test_web_state.get();

    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(0, std::move(test_web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::TestWebState>();
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
  }

  void SetUpActiveWebState() { web_state_list_->ActivateWebStateAt(0); }

  PopupMenuMediator* mediator_incognito_;
  PopupMenuMediator* mediator_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  ToolbarTestWebState* web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id popup_menu_;
  // Mock refusing all calls except -setPopupMenuItems:.
  id popup_menu_strict_;
};

// Tests that the feature engagement tracker get notified when the mediator is
// disconnected and the tracker wants the notification badge displayed.
TEST_F(PopupMenuMediatorTest, TestFeatureEngagementDisconnect) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  mediator_.popupMenu = popup_menu_;
  mediator_.engagementTracker = &tracker;

  EXPECT_CALL(tracker, Dismissed(testing::_));
  [mediator_ disconnect];
}
