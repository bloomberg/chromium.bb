// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_mediator.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
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
    mediator_.webStateList = web_state_list_.get();
    consumer_ = [OCMockObject mockForProtocol:@protocol(TabCollectionConsumer)];
    mediator_.consumer = consumer_;
  }
  ~TabCollectionMediatorTest() override { [mediator_ disconnect]; }

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
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state));
  }

  web::TestWebState* GetWebStateAt(int index) {
    return static_cast<web::TestWebState*>(
        web_state_list_->GetWebStateAt(index));
  }

  TabCollectionMediator* mediator_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id consumer_;
};

// Tests that -numberOfTabs returns the expected number of elements from
// web_state_list_.
TEST_F(TabCollectionMediatorTest, TestNumberOfTabs) {
  EXPECT_EQ(3, [mediator_ numberOfTabs]);
}

// Tests that -indexOfActiveTab returns the expected active_index from
// web_state_list_.
TEST_F(TabCollectionMediatorTest, TestActiveTabIndex) {
  EXPECT_EQ(0, [mediator_ indexOfActiveTab]);
}

// Tests that -titleAtIndex: returns the expected title from web_state_list_.
TEST_F(TabCollectionMediatorTest, TestTitleAtIndex) {
  EXPECT_NSEQ(@"http://test/0", [mediator_ titleAtIndex:0]);
}

// Tests that the consumer is notified of an insert into webStateList.
TEST_F(TabCollectionMediatorTest, TestInsertWebState) {
  [[consumer_ expect] insertItemAtIndex:2];
  InsertWebState(2);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified that a web state has been moved in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestMoveWebState) {
  NSMutableIndexSet* indexes = [NSMutableIndexSet indexSet];
  [indexes addIndex:0];
  [indexes addIndex:1];
  [indexes addIndex:2];
  [[consumer_ expect] reloadItemsAtIndexes:indexes];
  web_state_list_->MoveWebStateAt(0, 2);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified that a web state has been replaced in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestReplaceWebState) {
  NSIndexSet* indexes = [NSIndexSet indexSetWithIndex:1];
  [[consumer_ expect] reloadItemsAtIndexes:indexes];
  auto different_web_state = base::MakeUnique<web::TestWebState>();
  web_state_list_->ReplaceWebStateAt(1, std::move(different_web_state));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified that a web state has been deleted from
// webStateList.
TEST_F(TabCollectionMediatorTest, TestDetachWebState) {
  [[consumer_ expect] deleteItemAtIndex:1];
  web_state_list_->CloseWebStateAt(1);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified that the active web state has changed in
// webStateList.
TEST_F(TabCollectionMediatorTest, TestChangeActiveWebState) {
  NSMutableIndexSet* indexes = [NSMutableIndexSet indexSet];
  [indexes addIndex:0];
  [indexes addIndex:2];
  [[consumer_ expect] reloadItemsAtIndexes:indexes];
  web_state_list_->ActivateWebStateAt(2);
  EXPECT_OCMOCK_VERIFY(consumer_);
}
