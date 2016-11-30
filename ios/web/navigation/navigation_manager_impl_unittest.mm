// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/public/test/test_browser_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {
// Stub class for NavigationManagerDelegate.
class TestNavigationManagerDelegate : public NavigationManagerDelegate {
  void GoToIndex(int index) override {}
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override {}
  void OnNavigationItemsPruned(size_t pruned_item_count) override {}
  void OnNavigationItemChanged() override{};
  void OnNavigationItemCommitted(const LoadCommittedDetails&) override {}
  WebState* GetWebState() override { return nullptr; }
};
}  // namespace

// Test fixture for NavigationManagerImpl testing.
class NavigationManagerTest : public PlatformTest {
 protected:
  NavigationManagerTest()
      : manager_(new NavigationManagerImpl(&delegate_, &browser_state_)) {
    controller_.reset([[CRWSessionController alloc]
           initWithWindowName:nil
                     openerId:nil
                  openedByDOM:NO
        openerNavigationIndex:0
                 browserState:&browser_state_]);
    manager_->SetSessionController(controller_.get());
  }
  CRWSessionController* session_controller() { return controller_.get(); }
  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

 private:
  TestBrowserState browser_state_;
  TestNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;
  base::scoped_nsobject<CRWSessionController> controller_;
};

// Tests state of an empty navigation manager.
TEST_F(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetCurrentItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPreviousItem());
  EXPECT_EQ(-1, navigation_manager()->GetCurrentItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns current item index if there is a
// pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns same index as was set by
// -[CRWSessionController setPendingEntryIndex:].
TEST_F(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  [session_controller() setPendingEntryIndex:0];
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_F(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItem) {
  [session_controller() addTransientEntryWithURL:GURL("http://www.url.com")];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addTransientEntryWithURL:GURL("http://www.url.com/0")];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/1")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToEntryAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToEntryAtIndex:0];
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToEntryAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible if there is a
// pending entry.
TEST_F(NavigationManagerTest, CanGoForwardWithPendingItem) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() goToEntryAtIndex:0];
  [session_controller() addPendingEntry:GURL("http://www.url.com/1")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];

  // Pending entry should not allow going forward.
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_F(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  [session_controller() addPendingEntry:GURL("http://www.url.com")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/1")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_TYPED
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToEntryAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToEntryAtIndex:0];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToEntryAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToEntryAtIndex:2];
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_F(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  [session_controller() addPendingEntry:GURL("http://www.url.com/0")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_LINK
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/redirect")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_IS_REDIRECT_MASK
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/1")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_LINK
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/2")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_LINK
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  [session_controller() addPendingEntry:GURL("http://www.url.com/redirect")
                               referrer:Referrer()
                             transition:ui::PAGE_TRANSITION_IS_REDIRECT_MASK
                      rendererInitiated:NO];
  [session_controller() commitPendingEntry];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());

  // Go to entry at index 1 and test API from that state.
  [session_controller() goToEntryAtIndex:1];
  ASSERT_EQ(1, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 2 and test API from that state.
  [session_controller() goToEntryAtIndex:2];
  ASSERT_EQ(2, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 4 and test API from that state.
  [session_controller() goToEntryAtIndex:4];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry.
  [session_controller() addTransientEntryWithURL:GURL("http://www.url.com")];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Now test with pending item index.
  [session_controller() discardNonCommittedEntries];

  // Set pending index to 1 and test API from that state.
  [session_controller() setPendingEntryIndex:1];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 2 and test API from that state.
  [session_controller() setPendingEntryIndex:2];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(2, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 4 and committed entry to 1 and test.
  [session_controller() goToEntryAtIndex:1];
  [session_controller() setPendingEntryIndex:4];
  ASSERT_EQ(1, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(4, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry.
  [session_controller() addTransientEntryWithURL:GURL("http://www.url.com")];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(1, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(4, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));
}

}  // namespace web
