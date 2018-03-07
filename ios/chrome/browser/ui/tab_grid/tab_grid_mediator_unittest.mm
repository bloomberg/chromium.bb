// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/mac/foundation_util.h"
#include "base/test/scoped_task_environment.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid_item.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TabGridMediatorTest : public PlatformTest {
 public:
  TabGridMediatorTest() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    tab_model_ = OCMClassMock([TabModel class]);
    OCMStub([tab_model_ webStateList]).andReturn(web_state_list_.get());

    // Insert some web states.
    for (int i = 0; i < 3; i++) {
      auto web_state = std::make_unique<web::TestWebState>();
      TabIdTabHelper::CreateForWebState(web_state.get());
      web_state_list_->InsertWebState(i, std::move(web_state),
                                      WebStateList::INSERT_FORCE_INDEX,
                                      WebStateOpener());
    }
    web_state_list_->ActivateWebStateAt(1);

    consumer_ = OCMProtocolMock(@protocol(GridConsumer));
    mediator_ = [[TabGridMediator alloc] initWithConsumer:consumer_];
    mediator_.tabModel = tab_model_;
  }
  ~TabGridMediatorTest() override {}

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id tab_model_;
  id consumer_;
  TabGridMediator* mediator_;
};

// Tests that the consumer is populated after the tab model is set on the
// mediator.
TEST_F(TabGridMediatorTest, ConsumerPopulateItems) {
  [[consumer_ verify] populateItems:[OCMArg checkWithBlock:^BOOL(id value) {
                        NSArray* items =
                            base::mac::ObjCCastStrict<NSArray>(value);
                        EXPECT_EQ(3UL, items.count);
                        return YES;
                      }]
                      selectedIndex:1];
}

// Tests that the consumer is notified when a web state is inserted.
TEST_F(TabGridMediatorTest, ConsumerInsertItem) {
  auto web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(web_state.get());
  NSString* item_identifier =
      TabIdTabHelper::FromWebState(web_state.get())->tab_id();
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  [[consumer_ verify] insertItem:[OCMArg checkWithBlock:^BOOL(id value) {
                        GridItem* item =
                            base::mac::ObjCCastStrict<GridItem>(value);
                        EXPECT_NSEQ(item_identifier, item.identifier);
                        return YES;
                      }]
                         atIndex:1
                   selectedIndex:2];
}

// Tests that the consumer is notified when a web state is removed.
TEST_F(TabGridMediatorTest, ConsumerRemoveItem) {
  web_state_list_->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  [[consumer_ verify] removeItemAtIndex:1 selectedIndex:1];
}

// Tests that the consumer is notified when the active web state is changed.
TEST_F(TabGridMediatorTest, ConsumerUpdateSelectedItem) {
  // Selected index is 1 before the update.
  web_state_list_->ActivateWebStateAt(2);
  [[consumer_ verify] selectItemAtIndex:2];
}

// Tests that the consumer is notified when a web state is replaced.
TEST_F(TabGridMediatorTest, ConsumerReplaceItem) {
  auto new_web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(new_web_state.get());
  NSString* new_item_identifier =
      TabIdTabHelper::FromWebState(new_web_state.get())->tab_id();
  web_state_list_->ReplaceWebStateAt(1, std::move(new_web_state));
  [[consumer_ verify]
      replaceItemAtIndex:1
                withItem:[OCMArg checkWithBlock:^BOOL(id value) {
                  GridItem* item = base::mac::ObjCCastStrict<GridItem>(value);
                  EXPECT_NSEQ(new_item_identifier, item.identifier);
                  return YES;
                }]];
}

// Tests that the consumer is notified when a web state is moved.
TEST_F(TabGridMediatorTest, ConsumerMoveItem) {
  // Selected index is 1 before the move.
  web_state_list_->MoveWebStateAt(1, 2);
  [[consumer_ verify] moveItemFromIndex:1 toIndex:2 selectedIndex:2];
}

// Tests that the active index is updated when |-selectItemAtIndex:| is called.
TEST_F(TabGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  [mediator_ selectItemAtIndex:2];
  EXPECT_EQ(2, web_state_list_->active_index());
}

// Tests that the webStateList count is decremented when |-closeItemAtIndex:| is
// called.
TEST_F(TabGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  [mediator_ closeItemAtIndex:1];
  EXPECT_EQ(2, web_state_list_->count());
}

// Tests that the webStateList is empty when |-closeAllItems| is called.
TEST_F(TabGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, web_state_list_->count());
}
