// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_consumer.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TabCollectionMediatorTest : public PlatformTest {
 public:
  TabCollectionMediatorTest() {
    SetUpWebStateList();
    mediator_ = [[TabCollectionMediator alloc] init];
    snapshot_cache_ = [[SnapshotCache alloc] init];
    mediator_.snapshotCache = snapshot_cache_;
    mediator_.webStateList = web_state_list_.get();
    consumer_ = OCMProtocolMock(@protocol(TabCollectionConsumer));
    mediator_.consumer = consumer_;
  }
  ~TabCollectionMediatorTest() override {
    [mediator_ disconnect];
    [snapshot_cache_ shutdown];
  }

 protected:
  void SetUpWebStateList() {
    web_state_list_ = base::MakeUnique<WebStateList>(&web_state_list_delegate_);
    for (int i = 0; i < 3; i++) {
      InsertWebState(i);
    }
    web_state_list_->ActivateWebStateAt(0);
  }

  void InsertWebState(int index) {
    auto web_state = base::MakeUnique<web::TestWebState>();
    TabIdTabHelper::CreateForWebState(web_state.get());
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
  }

  web::TestWebState* GetWebStateAt(int index) {
    return static_cast<web::TestWebState*>(
        web_state_list_->GetWebStateAt(index));
  }

  NSString* GetTabIdAt(int index) {
    return TabIdTabHelper::FromWebState(GetWebStateAt(index))->tab_id();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  TabCollectionMediator* mediator_;
  SnapshotCache* snapshot_cache_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id consumer_;
};

// Tests that the consumer is notified of an insert into webStateList.
TEST_F(TabCollectionMediatorTest, TestInsertWebState) {
  InsertWebState(2);
  [[consumer_ verify] insertItem:[OCMArg any] atIndex:2 selectedIndex:0];
}

// Tests that the consumer is notified that a web state has been moved in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestMoveWebState) {
  web_state_list_->MoveWebStateAt(0, 2);
  [[consumer_ verify] moveItemFromIndex:0 toIndex:2 selectedIndex:2];
}

// Tests that the consumer is notified that a web state has been replaced in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestReplaceWebState) {
  auto different_web_state = base::MakeUnique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(different_web_state.get());
  web_state_list_->ReplaceWebStateAt(1, std::move(different_web_state));
  [[consumer_ verify] replaceItemAtIndex:1 withItem:[OCMArg any]];
}

// Tests that the consumer is notified that a web state has been deleted from
// webStateList.
TEST_F(TabCollectionMediatorTest, TestDetachWebState) {
  web_state_list_->CloseWebStateAt(1, WebStateList::CLOSE_USER_ACTION);
  [[consumer_ verify] deleteItemAtIndex:1 selectedIndex:0];
}

// Tests that the consumer is notified that the active web state has changed in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestChangeActiveWebState) {
  web_state_list_->ActivateWebStateAt(2);
  // Due to use of id for OCMock objects, naming collisions can exist. In this
  // case, the method -setSelectedIndex: collides with a property setter in
  // UIKit's UITabBarController class. The fix is to cast after calling -verify.
  auto consumer = static_cast<id<TabCollectionConsumer>>([consumer_ verify]);
  [consumer setSelectedIndex:2];
}

// Tests that the consumer is notified that a snapshot has been updated.
TEST_F(TabCollectionMediatorTest, TestUpdateSnapshot) {
  [snapshot_cache_ setImage:[[UIImage alloc] init] withSessionID:GetTabIdAt(0)];
  [[consumer_ verify] updateSnapshotAtIndex:0];
}
