// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include <string>

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/test/fakes/crw_test_back_forward_list.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// URL scheme that will be rewritten by UrlRewriter installed in
// NavigationManagerTest fixture. Scheme will be changed to kTestWebUIScheme.
const char kSchemeToRewrite[] = "navigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool UrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
  }
  return false;
}

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "navigationmanagerrewrittenquery";

// Appends |kRewrittenQueryParam| to |url|.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// Stub class for NavigationManagerDelegate.
class TestNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  bool reload_called() { return reload_called_; }
  bool record_page_state_called() { return record_page_state_called_; }
  void SetSessionController(CRWSessionController* session_controller) {
    session_controller_ = session_controller;
  }
  void SetWKWebView(id web_view) { mock_web_view_ = web_view; }

 private:
  // NavigationManagerDelegate overrides.
  void GoToIndex(int index) override {
    [session_controller_ goToItemAtIndex:index discardNonCommittedItems:NO];
  }
  void ClearTransientContent() override {}
  void RecordPageStateInNavigationItem() override {
    record_page_state_called_ = true;
  }
  void WillLoadCurrentItemWithParams(const NavigationManager::WebLoadParams&,
                                     bool is_initial_navigation) override {}
  void LoadCurrentItem() override {}
  void Reload() override { reload_called_ = true; }
  void OnNavigationItemsPruned(size_t pruned_item_count) override {}
  void OnNavigationItemChanged() override {}
  void OnNavigationItemCommitted(const LoadCommittedDetails&) override {}
  WebState* GetWebState() override { return nullptr; }
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override {
    return mock_web_view_;
  }

  bool reload_called_ = false;
  bool record_page_state_called_ = false;
  CRWSessionController* session_controller_;
  id mock_web_view_;
};
}  // namespace

// NavigationManagerTest is parameterized on this enum to test both the legacy
// implementation of navigation manager and the experimental implementation.
enum NavigationManagerChoice {
  TEST_LEGACY_NAVIGATION_MANAGER,
  TEST_WK_BASED_NAVIGATION_MANAGER,
};

// Programmatic test fixture for NavigationManagerImpl testing.
// GetParam() chooses whether to run tests on LegacyNavigationManagerImpl or
// WKBasedNavigationManagerImpl.
// TODO(crbug.com/734150): cleanup the LegacyNavigationManagerImpl use case.
class NavigationManagerTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  NavigationManagerTest() {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      manager_.reset(new LegacyNavigationManagerImpl);
      controller_ =
          [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
      delegate_.SetSessionController(session_controller());
    } else {
      manager_.reset(new WKBasedNavigationManagerImpl);
      mock_web_view_ = OCMClassMock([WKWebView class]);
      mock_wk_list_ = [[CRWTestBackForwardList alloc] init];
      OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
      delegate_.SetWKWebView(mock_web_view_);
    }
    // Setup rewriter.
    BrowserURLRewriter::GetInstance()->AddURLRewriter(UrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITHOUT_PORT);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);
    manager_->SetSessionController(controller_);
  }

  CRWSessionController* session_controller() { return controller_; }
  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

  TestNavigationManagerDelegate navigation_manager_delegate() {
    return delegate_;
  }

 protected:
  CRWTestBackForwardList* mock_wk_list_;
  WKWebView* mock_web_view_;

 private:
  TestBrowserState browser_state_;
  TestNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;
  CRWSessionController* controller_;
};

// Tests state of an empty navigation manager.
TEST_P(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
  EXPECT_EQ(-1, navigation_manager()->GetPreviousItemIndex());
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns current item index if there is a
// pending entry.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns same index as was set by
// -[CRWSessionController setPendingItemIndex:].
TEST_P(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:0];
    EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
  }
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_P(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  // Setup a pending item for which a transient item can be added.
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }
  navigation_manager()->GoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  navigation_manager()->GoToIndex(0);
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  navigation_manager()->GoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_P(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }
  navigation_manager()->GoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  navigation_manager()->GoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  navigation_manager()->GoToIndex(2);
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_P(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1", @"http://www.url.com/2"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }
  // Go to entry at index 1 and test API from that state.
  navigation_manager()->GoToIndex(1);
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
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
  navigation_manager()->GoToIndex(2);
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
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
  navigation_manager()->GoToIndex(4);
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  navigation_manager()->DiscardNonCommittedItems();

  // Set pending index to 1 and test API from that state.
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:2];
  }
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  navigation_manager()->GoToIndex(1);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:4];
  }
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
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

  // Test with existing transient entry in the end of the stack.
  navigation_manager()->GoToIndex(4);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:-1];
  }
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
}

// Tests offsets with pending transient entries (specifically gong back and
// forward from a pending navigation entry that is added to the middle of the
// navigation stack).
TEST_P(NavigationManagerTest, OffsetsWithPendingTransientEntry) {
  // This test directly manipulates the WKBackForwardListItem mocks stored in
  // mock_wk_list_ so that the associated NavigationItem objects are retained
  // throughout the test case.
  WKBackForwardListItem* wk_item0 =
      [CRWTestBackForwardList itemWithURLString:@"http://www.url.com/0"];
  WKBackForwardListItem* wk_item1 =
      [CRWTestBackForwardList itemWithURLString:@"http://www.url.com/1"];
  WKBackForwardListItem* wk_item2 =
      [CRWTestBackForwardList itemWithURLString:@"http://www.url.com/2"];

  // Create a transient item in the middle of the navigation stack and go back
  // to it (pending index is 1, current index is 2).
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item0;
  mock_wk_list_.backList = nil;
  mock_wk_list_.forwardList = nil;
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item2;
  mock_wk_list_.backList = @[ wk_item0, wk_item1 ];
  navigation_manager()->CommitPendingItem();

  // Under back-forward navigation, both WKBackForwardList and WKWebView.URL are
  // updated before |didStartProvisionalNavigation| callback, which calls
  // AddPendingItem. Simulate this behavior.
  OCMExpect([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.url.com/1"]);
  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  mock_wk_list_.forwardList = @[ wk_item2 ];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }

  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }

  // Now go forward to that middle transient item (pending index is 1,
  // current index is 0).
  navigation_manager()->GoToIndex(0);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));
}

// Tests that when given a pending item, adding a new pending item replaces the
// existing pending item if their URLs are different.
TEST_P(NavigationManagerTest, ReplacePendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(existing_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if new pending item is not a
// form submission.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_TYPED));
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if new pending item is a form
// submission while existing pending item is not.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if the user agent override
// option is INHERIT.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_TYPED));
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// DESKTOP.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// MOBILE.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item
// succeeds if the new item's URL is different from the last committed item.
TEST_P(NavigationManagerTest, AddPendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(existing_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the new item is not form submission.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_FALSE(navigation_manager()->GetPendingItem());
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    ASSERT_TRUE(navigation_manager()->GetPendingItem());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the new item is a form submission while the last
// committed item is not.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  // Add if new transition is a form submission.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if both the new item and the last committed item are form
// submissions.
TEST_P(NavigationManagerTest,
       NotAddSameUrlPendingItemIfDuplicateFormSubmission) {
  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test when form submission check is
    // implemented.
    return;
  }
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the user agent override option is INHERIT.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_FALSE(navigation_manager()->GetPendingItem());
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    ASSERT_TRUE(navigation_manager()->GetPendingItem());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is DESKTOP.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is MOBILE.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that desktop user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is DESKTOP.
TEST_P(NavigationManagerTest, OverrideUserAgentWithDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(UserAgentType::MOBILE, last_committed_item->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that mobile user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is MOBILE.
TEST_P(NavigationManagerTest, OverrideUserAgentWithMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  last_committed_item->SetUserAgentType(UserAgentType::DESKTOP);
  EXPECT_EQ(UserAgentType::DESKTOP, last_committed_item->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that the UserAgentType of an INHERIT item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterInherit) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a MOBILE item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a DESKTOP item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems if
// a native URL exists in between navigations.
TEST_P(NavigationManagerTest, UserAgentTypePropagationPastNativeItems) {
  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |AddPendingItem| is
    // integrated with native views.
    return;
  }
  // GURL::Replacements that will replace a GURL's scheme with the test native
  // scheme.
  GURL::Replacements native_scheme_replacement;
  native_scheme_replacement.SetSchemeStr(kTestNativeContentScheme);

  // Create two non-native navigations that are separated by a native one.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType());

  GURL item2_url = item1->GetURL().ReplaceComponents(native_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item2_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(item2_url.spec())
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  web::NavigationItem* native_item1 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.2.com"
         backListURLs:@[
           @"http://www.1.com", base::SysUTF8ToNSString(item2_url.spec())
         ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();
  web::NavigationItem* item2 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item1|'s UserAgentType is propagated to |item2|.
  EXPECT_EQ(item1->GetUserAgentType(), item2->GetUserAgentType());

  // Update |item2|'s UA type to DESKTOP and add a third non-native navigation,
  // once again separated by a native one.
  item2->SetUserAgentType(web::UserAgentType::DESKTOP);
  ASSERT_EQ(web::UserAgentType::DESKTOP, item2->GetUserAgentType());

  GURL item3_url = item2->GetURL().ReplaceComponents(native_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item3_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:base::SysUTF8ToNSString(item3_url.spec())
         backListURLs:@[
           @"http://www.1.com", base::SysUTF8ToNSString(item2_url.spec()),
           @"http://www.2.com"
         ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();
  web::NavigationItem* native_item2 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.3.com"
         backListURLs:@[
           @"http://www.1.com", base::SysUTF8ToNSString(item2_url.spec()),
           @"http://www.2.com", base::SysUTF8ToNSString(item3_url.spec())
         ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();
  web::NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item2|'s UserAgentType is propagated to |item3|.
  EXPECT_EQ(item2->GetUserAgentType(), item3->GetUserAgentType());
}

// Tests that adding transient item for a pending item with mobile user agent
// type results in a transient item with mobile user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForMobilePendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::MOBILE);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that adding transient item for a pending item with desktop user agent
// type results in a transient item with desktop user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForDesktopPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::DESKTOP);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL is no-op when there
// are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithNormalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_FALSE(navigation_manager_delegate().reload_called());

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the renderer initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the user initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithNormalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item, but there
// forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithNormalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }

  navigation_manager()->GoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL is
// no-op when there are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithOriginalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_FALSE(navigation_manager_delegate().reload_called());

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the renderer initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the user initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item, but there are forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithOriginalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/1/original"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }
  navigation_manager()->GoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that app-specific URLs are not rewritten for renderer-initiated loads
// unless requested by a page with app-specific url.
TEST_P(NavigationManagerTest, RewritingAppSpecificUrls) {
  // URL should not be rewritten as there is no committed URL.
  GURL url1(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url1, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url1, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten because last committed URL is not app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url1.spec())];
  navigation_manager()->CommitPendingItem();

  GURL url2(url::SchemeHostPort(kSchemeToRewrite, "test2", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url2, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten for user initiated navigations.
  GURL url3(url::SchemeHostPort(kSchemeToRewrite, "test3", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url3, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url3(
      url::SchemeHostPort(kTestWebUIScheme, "test3", 0).Serialize());
  EXPECT_EQ(rewritten_url3, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten because last committed URL is app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(rewritten_url3.spec())
                  backListURLs:@[ base::SysUTF8ToNSString(url1.spec()) ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  GURL url4(url::SchemeHostPort(kSchemeToRewrite, "test4", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url4, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url4(
      url::SchemeHostPort(kTestWebUIScheme, "test4", 0).Serialize());
  EXPECT_EQ(rewritten_url4, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that transient URLRewriters are applied for pending items.
TEST_P(NavigationManagerTest, ApplyTransientRewriters) {
  navigation_manager()->AddTransientURLRewriter(&AppendingUrlRewriter);
  navigation_manager()->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(kRewrittenQueryParam, pending_item->GetURL().query());

  // Now that the transient rewriters are consumed, the next URL should not be
  // changed.
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetIndexOfItem() returns the correct values.
TEST_P(NavigationManagerTest, GetIndexOfItem) {
  // This test manipuates the WKBackForwardListItems in mock_wk_list_ directly
  // to retain the NavigationItem association.
  WKBackForwardListItem* wk_item0 =
      [CRWTestBackForwardList itemWithURLString:@"http://www.url.com/0"];
  WKBackForwardListItem* wk_item1 =
      [CRWTestBackForwardList itemWithURLString:@"http://www.url.com/1"];

  // Create two items and add them to the NavigationManagerImpl.
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item0;
  navigation_manager()->CommitPendingItem();

  web::NavigationItem* item0 = navigation_manager()->GetLastCommittedItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  navigation_manager()->CommitPendingItem();

  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  // Create an item that does not exist in the NavigationManagerImpl.
  std::unique_ptr<web::NavigationItem> item_not_found =
      web::NavigationItem::Create();
  // Verify GetIndexOfItem() results.
  EXPECT_EQ(0, navigation_manager()->GetIndexOfItem(item0));
  EXPECT_EQ(1, navigation_manager()->GetIndexOfItem(item1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexOfItem(item_not_found.get()));
}

// Tests that GetBackwardItems() and GetForwardItems() return expected entries
// when current item is in the middle of the navigation history.
TEST_P(NavigationManagerTest, TestBackwardForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(2U, back_items.size());
  EXPECT_EQ("http://www.url.com/1", back_items[0]->GetURL().spec());
  EXPECT_EQ("http://www.url.com/0", back_items[1]->GetURL().spec());
  EXPECT_TRUE(navigation_manager()->GetForwardItems().empty());

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test once |GoToIndex| is
    // implemented in WKBasedNavigationManager.
    return;
  }
  navigation_manager()->GoBack();
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
  NavigationItemList forward_items = navigation_manager()->GetForwardItems();
  EXPECT_EQ(1U, forward_items.size());
  EXPECT_EQ("http://www.url.com/2", forward_items[0]->GetURL().spec());
}

// Tests that pending item is not considered part of session history so that
// GetBackwardItems returns the second last committed item even if there is a
// pendign item.
TEST_P(NavigationManagerTest, NewPendingItemIsHiddenFromHistory) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

// Tests that all committed items are considered history if there is a transient
// item. This can happen if an intersitial was loaded for SSL error.
// See crbug.com/691311.
TEST_P(NavigationManagerTest,
       BackwardItemsShouldContainAllCommittedIfCurrentIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

TEST_P(NavigationManagerTest, BackwardItemsShouldBeEmptyIfFirstIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_TRUE(back_items.empty());
}

TEST_P(NavigationManagerTest, VisibleItemIsTransientItemIfPresent) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still transient item even if there is a committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/3"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/3",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, PendingItemIsVisibleIfNewAndUserInitiated) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still the user initiated pending item even if there is a
  // committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, PendingItemIsNotVisibleIfNotUserInitiated) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(nullptr, navigation_manager()->GetVisibleItem());
}

TEST_P(NavigationManagerTest, PendingItemIsNotVisibleIfNotNewNavigation) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  // Move pending item back to index 0.
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:0];
  } else {
    OCMExpect([mock_web_view_ URL])
        .andReturn([NSURL URLWithString:@"http://www.url.com/0"]);
    [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                    backListURLs:nil
                 forwardListURLs:@[ @"http://www.url.com/1" ]];
    navigation_manager()->AddPendingItem(
        GURL("http://www.url.com/0"), Referrer(),
        ui::PAGE_TRANSITION_FORWARD_BACK,
        web::NavigationInitiationType::USER_INITIATED,
        web::NavigationManager::UserAgentOverrideOption::INHERIT);
  }
  ASSERT_EQ(0, navigation_manager()->GetPendingItemIndex());

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, VisibleItemDefaultsToLastCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

// Tests that |extra_headers| and |post_data| from WebLoadParams are added to
// the new navigation item if they are present.
TEST_P(NavigationManagerTest, LoadURLWithParamsWithExtraHeadersAndPostData) {
  NavigationManager::WebLoadParams params(GURL("http://www.url.com/0"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers.reset(@{@"Content-Type" : @"text/plain"});
  params.post_data.reset([NSData data]);
  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();

  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/0", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_NSEQ(pending_item->GetHttpRequestHeaders(),
              @{@"Content-Type" : @"text/plain"});
  EXPECT_TRUE(pending_item->HasPostData());
}

// Tests that LoadURLWithParams() calls RecordPageStateInNavigationItem() on the
// navigation manager deleget before navigating to the new URL.
TEST_P(NavigationManagerTest, LoadURLWithParamsSavesStateOnCurrentItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  NavigationManager::WebLoadParams params(GURL("http://www.url.com/1"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_TRUE(last_committed_item);
  EXPECT_EQ("http://www.url.com/0", last_committed_item->GetURL().spec());
  EXPECT_TRUE(navigation_manager_delegate().record_page_state_called());

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/1", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(pending_item->HasPostData());
}

INSTANTIATE_TEST_CASE_P(
    ProgrammaticNavigationManagerTest,
    NavigationManagerTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web
