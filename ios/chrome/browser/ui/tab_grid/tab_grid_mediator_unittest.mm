// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test object that conforms to GridConsumer and exposes inner state for test
// verification.
@interface FakeConsumer : NSObject<GridConsumer>
@property(nonatomic, strong) NSMutableArray<GridItem*>* items;
@property(nonatomic, assign) NSUInteger selectedIndex;
@end
@implementation FakeConsumer
@synthesize items = _items;
@synthesize selectedIndex = _selectedIndex;

- (void)populateItems:(NSArray<GridItem*>*)items
        selectedIndex:(NSUInteger)selectedIndex {
  self.selectedIndex = selectedIndex;
  self.items = [items mutableCopy];
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
     selectedIndex:(NSUInteger)selectedIndex {
  [self.items insertObject:item atIndex:index];
  self.selectedIndex = selectedIndex;
}

- (void)removeItemAtIndex:(NSUInteger)index
            selectedIndex:(NSUInteger)selectedIndex {
  [self.items removeObjectAtIndex:index];
  self.selectedIndex = selectedIndex;
}

- (void)selectItemAtIndex:(NSUInteger)selectedIndex {
  self.selectedIndex = selectedIndex;
}

- (void)replaceItemAtIndex:(NSUInteger)index withItem:(GridItem*)item {
  self.items[index] = item;
}

- (void)moveItemFromIndex:(NSUInteger)fromIndex
                  toIndex:(NSUInteger)toIndex
            selectedIndex:(NSUInteger)selectedIndex {
  GridItem* item = self.items[fromIndex];
  [self.items removeObjectAtIndex:fromIndex];
  [self.items insertObject:item atIndex:toIndex];
  self.selectedIndex = selectedIndex;
}

@end

// Fake WebStateList delegate that attaches the tab ID tab helper.
class TabIdFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  explicit TabIdFakeWebStateListDelegate(web::WebStateDelegate* delegate)
      : delegate_(delegate) {}
  ~TabIdFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    TabIdTabHelper::CreateForWebState(web_state);
    web_state->SetDelegate(delegate_);
  }
  void WebStateDetached(web::WebState* web_state) override {
    web_state->SetDelegate(nil);
  }

 private:
  web::WebStateDelegate* delegate_;
};

class TabGridMediatorTest : public PlatformTest {
 public:
  TabGridMediatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web_state_list_delegate_ =
        std::make_unique<TabIdFakeWebStateListDelegate>(&delegate_);
    web_state_list_ =
        std::make_unique<WebStateList>(web_state_list_delegate_.get());
    tab_model_ = OCMClassMock([TabModel class]);
    OCMStub([tab_model_ webStateList]).andReturn(web_state_list_.get());
    OCMStub([tab_model_ browserState]).andReturn(browser_state_.get());
    original_identifiers_ = [[NSMutableSet alloc] init];

    // Insert some web states.
    for (int i = 0; i < 3; i++) {
      auto web_state = std::make_unique<web::TestWebState>();
      TabIdTabHelper::CreateForWebState(web_state.get());
      NSString* identifier =
          TabIdTabHelper::FromWebState(web_state.get())->tab_id();
      [original_identifiers_ addObject:identifier];
      web_state_list_->InsertWebState(i, std::move(web_state),
                                      WebStateList::INSERT_FORCE_INDEX,
                                      WebStateOpener());
    }
    web_state_list_->ActivateWebStateAt(1);
    consumer_ = [[FakeConsumer alloc] init];
    mediator_ = [[TabGridMediator alloc] initWithConsumer:consumer_];
    mediator_.tabModel = tab_model_;
  }
  ~TabGridMediatorTest() override {}

 protected:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  web::TestWebStateDelegate delegate_;
  std::unique_ptr<TabIdFakeWebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id tab_model_;
  FakeConsumer* consumer_;
  TabGridMediator* mediator_;
  NSMutableSet* original_identifiers_;
};

// Tests that the consumer is populated after the tab model is set on the
// mediator.
TEST_F(TabGridMediatorTest, ConsumerPopulateItems) {
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_EQ(1UL, consumer_.selectedIndex);
}

// Tests that the consumer is notified when a web state is inserted.
TEST_F(TabGridMediatorTest, ConsumerInsertItem) {
  ASSERT_EQ(3UL, consumer_.items.count);
  ASSERT_EQ(1UL, consumer_.selectedIndex);
  auto web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(web_state.get());
  NSString* item_identifier =
      TabIdTabHelper::FromWebState(web_state.get())->tab_id();
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_EQ(2UL, consumer_.selectedIndex);
  EXPECT_NSEQ(item_identifier, consumer_.items[1].identifier);
  EXPECT_FALSE([original_identifiers_ containsObject:item_identifier]);
}

// Tests that the consumer is notified when a web state is removed.
TEST_F(TabGridMediatorTest, ConsumerRemoveItem) {
  web_state_list_->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(2UL, consumer_.items.count);
  EXPECT_EQ(1UL, consumer_.selectedIndex);
}

// Tests that the consumer is notified when the active web state is changed.
TEST_F(TabGridMediatorTest, ConsumerUpdateSelectedItem) {
  ASSERT_EQ(1UL, consumer_.selectedIndex);
  web_state_list_->ActivateWebStateAt(2);
  EXPECT_EQ(2UL, consumer_.selectedIndex);
}

// Tests that the consumer is notified when a web state is replaced.
TEST_F(TabGridMediatorTest, ConsumerReplaceItem) {
  auto new_web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(new_web_state.get());
  NSString* new_item_identifier =
      TabIdTabHelper::FromWebState(new_web_state.get())->tab_id();
  web_state_list_->ReplaceWebStateAt(1, std::move(new_web_state));
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_EQ(1UL, consumer_.selectedIndex);
  EXPECT_NSEQ(new_item_identifier, consumer_.items[1].identifier);
  EXPECT_FALSE([original_identifiers_ containsObject:new_item_identifier]);
}

// Tests that the consumer is notified when a web state is moved.
TEST_F(TabGridMediatorTest, ConsumerMoveItem) {
  NSString* item1 = consumer_.items[1].identifier;
  NSString* item2 = consumer_.items[2].identifier;
  ASSERT_EQ(1UL, consumer_.selectedIndex);
  web_state_list_->MoveWebStateAt(1, 2);
  EXPECT_NSEQ(item1, consumer_.items[2].identifier);
  EXPECT_NSEQ(item2, consumer_.items[1].identifier);
  EXPECT_EQ(2UL, consumer_.selectedIndex);
}

// Tests that the active index is updated when |-selectItemAtIndex:| is called.
TEST_F(TabGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  [mediator_ selectItemAtIndex:2];
  EXPECT_EQ(2, web_state_list_->active_index());
}

// Tests that the |web_state_list_| count is decremented when
// |-closeItemAtIndex:| is called.
TEST_F(TabGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  [mediator_ closeItemAtIndex:1];
  EXPECT_EQ(2, web_state_list_->count());
}

// Tests that the |web_state_list_| is empty when |-closeAllItems| is called.
TEST_F(TabGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, web_state_list_->count());
}

// Tests that when |-addNewItem| is called, the |web_state_list_| count is
// incremented, the |active_index| is the newly added index, the new web state
// has no opener, and the URL is the new tab page.
TEST_F(TabGridMediatorTest, AddNewItemAtEndCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ addNewItem];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(3, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(3);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  web::TestOpenURLRequest* request = delegate_.last_open_url_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(kChromeUINewTabURL, request->params.url.spec());
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
}

// Tests that when |-insertNewItemAtIndex:| is called, the |web_state_list_|
// count is incremented, the |active_index| is the newly added index, the new
// web state has no opener, and the URL is the new tab page.
TEST_F(TabGridMediatorTest, InsertNewItemCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(0, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  web::TestOpenURLRequest* request = delegate_.last_open_url_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(kChromeUINewTabURL, request->params.url.spec());
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
}
