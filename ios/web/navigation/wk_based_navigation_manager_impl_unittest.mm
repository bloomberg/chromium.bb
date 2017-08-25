// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#include <WebKit/WebKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/crw_test_back_forward_list.h"
#include "ios/web/test/fakes/test_navigation_manager_delegate.h"
#include "ios/web/test/test_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/page_transition_types.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "wknavigationmanagerrewrittenquery";

// Appends |kRewrittenQueryParam| to |url|.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// URL scheme that will be rewritten by WebUIUrlRewriter.
const char kSchemeToRewrite[] = "wknavigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool WebUIUrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
  }
  return false;
}

// Test fixture for WKBasedNavigationManagerImpl.
class WKBasedNavigationManagerTest : public PlatformTest {
 protected:
  WKBasedNavigationManagerTest() : manager_(new WKBasedNavigationManagerImpl) {
    mock_web_view_ = OCMClassMock([WKWebView class]);
    mock_wk_list_ = [[CRWTestBackForwardList alloc] init];
    OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
    delegate_.SetWebViewNavigationProxy(mock_web_view_);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);

    BrowserURLRewriter::GetInstance()->AddURLRewriter(WebUIUrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITHOUT_PORT);
  }

 protected:
  std::unique_ptr<NavigationManagerImpl> manager_;
  CRWTestBackForwardList* mock_wk_list_;
  id mock_web_view_;

 private:
  TestBrowserState browser_state_;
  TestNavigationManagerDelegate delegate_;
};

// Tests that GetItemAtIndex() on an empty manager will sync navigation items to
// WKBackForwardList using default properties.
TEST_F(WKBasedNavigationManagerTest, SyncAfterItemAtIndex) {
  EXPECT_EQ(0, manager_->GetItemCount());
  EXPECT_EQ(nullptr, manager_->GetItemAtIndex(0));

  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  EXPECT_EQ(1, manager_->GetItemCount());
  EXPECT_EQ(0, manager_->GetLastCommittedItemIndex());

  NavigationItem* item = manager_->GetItemAtIndex(0);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(GURL("http://www.0.com"), item->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item->GetUserAgentType());
  EXPECT_FALSE(item->GetTimestamp().is_null());
}

// Tests that Referrer is inferred from the previous WKBackForwardListItem.
TEST_F(WKBasedNavigationManagerTest, SyncAfterItemAtIndexWithPreviousItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:@[ @"http://www.2.com" ]];
  EXPECT_EQ(3, manager_->GetItemCount());
  EXPECT_EQ(1, manager_->GetLastCommittedItemIndex());

  // The out-of-order access is intentionall to test that syncing doesn't rely
  // on the previous WKBackForwardListItem having an associated NavigationItem.
  NavigationItem* item2 = manager_->GetItemAtIndex(2);
  ASSERT_NE(item2, nullptr);
  EXPECT_EQ(GURL("http://www.2.com"), item2->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item2->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item2->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.1.com"), item2->GetReferrer().url);
  EXPECT_FALSE(item2->GetTimestamp().is_null());

  NavigationItem* item1 = manager_->GetItemAtIndex(1);
  ASSERT_NE(item1, nullptr);
  EXPECT_EQ(GURL("http://www.1.com"), item1->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item1->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item1->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);
  EXPECT_FALSE(item1->GetTimestamp().is_null());

  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  ASSERT_NE(item0, nullptr);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item0->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item0->GetUserAgentType());
  EXPECT_FALSE(item0->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView does not have a linked entry.
TEST_F(WKBasedNavigationManagerTest, SyncInGetLastCommittedItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  EXPECT_EQ(1, manager_->GetItemCount());

  NavigationItem* item = manager_->GetLastCommittedItem();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ("http://www.0.com/", item->GetURL().spec());
  EXPECT_FALSE(item->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView is an app-specific URL.
TEST_F(WKBasedNavigationManagerTest,
       SyncInGetLastCommittedItemForAppSpecificURL) {
  GURL url(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());

  // Verifies that the test URL is rewritten into an app-specific URL.
  manager_->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  NavigationItem* pending_item = manager_->GetPendingItem();
  ASSERT_TRUE(pending_item);
  ASSERT_TRUE(web::GetWebClient()->IsAppSpecificURL(pending_item->GetURL()));

  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url.spec())];
  NavigationItem* item = manager_->GetLastCommittedItem();

  ASSERT_NE(item, nullptr);
  EXPECT_EQ(url, item->GetURL());
  EXPECT_EQ(1, manager_->GetItemCount());
}

// Tests that CommitPendingItem() will sync navigation items to
// WKBackForwardList and the pending item NavigationItemImpl will be used.
TEST_F(WKBasedNavigationManagerTest, GetItemAtIndexAfterCommitPending) {
  // Simulate a main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  NavigationItem* pending_item0 = manager_->GetPendingItem();

  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  EXPECT_EQ(1, manager_->GetItemCount());
  NavigationItem* item = manager_->GetLastCommittedItem();
  EXPECT_EQ(pending_item0, item);
  EXPECT_EQ(GURL("http://www.0.com"), item->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item->GetTransitionType()));
  EXPECT_EQ(UserAgentType::DESKTOP, item->GetUserAgentType());

  // Simulate a second main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  NavigationItem* pending_item2 = manager_->GetPendingItem();

  // Simulate an iframe navigation between the two main frame navigations.
  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.0.com", @"http://www.1.com" ]
               forwardListURLs:nil];
  manager_->CommitPendingItem();

  EXPECT_EQ(3, manager_->GetItemCount());
  EXPECT_EQ(2, manager_->GetLastCommittedItemIndex());

  // This item is created by syncing.
  NavigationItem* item1 = manager_->GetItemAtIndex(1);
  EXPECT_EQ(GURL("http://www.1.com"), item1->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item1->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item1->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);

  // This item is created by CommitPendingItem.
  NavigationItem* item2 = manager_->GetItemAtIndex(2);
  EXPECT_EQ(pending_item2, item2);
  EXPECT_EQ(GURL("http://www.2.com"), item2->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item2->GetTransitionType()));
  EXPECT_EQ(UserAgentType::DESKTOP, item2->GetUserAgentType());
  EXPECT_EQ(GURL(""), item2->GetReferrer().url);
}

// Tests that AddPendingItem does not create a new NavigationItem if the new
// pending item is a back forward navigation.
TEST_F(WKBasedNavigationManagerTest, ReusePendingItemForHistoryNavigation) {
  // Simulate two regular navigations.
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  // Force sync NavigationItems.
  NavigationItem* original_item0 = manager_->GetItemAtIndex(0);
  manager_->GetItemAtIndex(1);

  // Simulate a back-forward navigation. Manually shuffle the objects in
  // mock_wk_list_ to avoid creating new WKBackForwardListItem mocks and
  // preserve existing NavigationItem associations.
  WKBackForwardListItem* wk_item0 = [mock_wk_list_ itemAtIndex:-1];
  WKBackForwardListItem* wk_item1 = [mock_wk_list_ itemAtIndex:0];
  mock_wk_list_.currentItem = wk_item0;
  mock_wk_list_.backList = nil;
  mock_wk_list_.forwardList = @[ wk_item1 ];
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.0.com"]);

  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  EXPECT_EQ(original_item0, manager_->GetPendingItem());
}

// Tests that transient URL rewriters are only applied to a new pending item.
TEST_F(WKBasedNavigationManagerTest,
       TransientURLRewritersOnlyUsedForPendingItem) {
  manager_->AddPendingItem(GURL("http://www.0.com"), Referrer(),
                           ui::PAGE_TRANSITION_TYPED,
                           NavigationInitiationType::USER_INITIATED,
                           NavigationManager::UserAgentOverrideOption::INHERIT);

  // Install transient URL rewriters.
  manager_->AddTransientURLRewriter(&AppendingUrlRewriter);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];

  // Transient URL rewriters do not apply to lazily synced items.
  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());

  // Transient URL rewriters do not apply to transient items.
  manager_->AddTransientItem(GURL("http://www.1.com"));
  EXPECT_EQ(GURL("http://www.1.com"), manager_->GetTransientItem()->GetURL());

  // Transient URL rewriters are applied to a new pending item.
  manager_->AddPendingItem(GURL("http://www.2.com"), Referrer(),
                           ui::PAGE_TRANSITION_TYPED,
                           NavigationInitiationType::USER_INITIATED,
                           NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(kRewrittenQueryParam, manager_->GetPendingItem()->GetURL().query());
}

// Tests DiscardNonCommittedItems discards both pending and transient items.
TEST_F(WKBasedNavigationManagerTest, DiscardNonCommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com"));

  EXPECT_NE(nullptr, manager_->GetPendingItem());
  EXPECT_NE(nullptr, manager_->GetTransientItem());

  manager_->DiscardNonCommittedItems();
  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

// Tests that in the absence of a transient item, going back is delegated to the
// underlying WKWebView without any sanity checks such as whether any back
// history exists.
TEST_F(WKBasedNavigationManagerTest, GoBackWithoutTransientItem) {
  // The cast is necessary because without it, compiler cannot disambiguate
  // between UIWebView's goBack and WKWebView's goBack. Not using C++ style cast
  // because it doesn't work on id type.
  [(WKWebView*)[mock_web_view_ expect] goBack];
  manager_->GoBack();
  [mock_web_view_ verify];
}

// Tests that going back from a transient item will discard the transient item
// without any navigations on the underlying WKBackForwardList.
TEST_F(WKBasedNavigationManagerTest, GoBackFromTransientItem) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com"));

  [(WKWebView*)[mock_web_view_ reject] goBack];
  manager_->GoBack();
  [mock_web_view_ verify];

  EXPECT_NE(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

// Tests that going forward is always delegated to the underlying WKWebView
// without any sanity checks such as whether any forward history exists.
TEST_F(WKBasedNavigationManagerTest, GoForward) {
  [(WKWebView*)[mock_web_view_ expect] goForward];
  manager_->GoForward();
  [mock_web_view_ verify];
}

// Tests that going forward clears uncommitted items.
TEST_F(WKBasedNavigationManagerTest, GoForwardShouldDiscardsUncommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com"));

  EXPECT_NE(nullptr, manager_->GetPendingItem());
  EXPECT_NE(nullptr, manager_->GetTransientItem());

  manager_->GoForward();

  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

}  // namespace web
