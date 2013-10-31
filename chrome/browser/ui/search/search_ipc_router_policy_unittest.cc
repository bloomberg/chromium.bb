// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SearchIPCRouterPolicyTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInstantExtendedAPI);
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());
  }

  SearchTabHelper* GetSearchTabHelper() {
    SearchTabHelper* search_tab_helper =
        SearchTabHelper::FromWebContents(web_contents());
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return search_tab_helper;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

TEST_F(SearchIPCRouterPolicyTest, ProcessVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessSetVoiceSearchSupport());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessFocusOmnibox) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessFocusOmnibox) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, SendSetPromoInformation) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldSendSetPromoInformation());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendSetPromoInformation) {
  // Send promo information only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldSendSetPromoInformation());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessDeleteMostVisitedItem) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessDeleteMostVisitedItem());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoMostVisitedDeletion) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessUndoMostVisitedDeletion());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoAllMostVisitedDeletions) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessUndoAllMostVisitedDeletions());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessLogEvent) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessLogEvent) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessChromeIdentityCheck) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessChromeIdentityCheck());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessChromeIdentityCheck) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessChromeIdentityCheck());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessNavigateToURL) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessNavigateToURL(true));
}

TEST_F(SearchIPCRouterPolicyTest, ProcessPasteIntoOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessPasteIntoOmniboxMsg) {
  // Process message only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);

  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessFocusOmnibox(true));
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessNavigateToURL(true));
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessDeleteMostVisitedItem());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessUndoMostVisitedDeletion());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessUndoAllMostVisitedDeletions());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessLogEvent());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForInactiveTab) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Assume the NTP is deactivated.
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessFocusOmnibox(false));
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessNavigateToURL(false));
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldProcessPasteIntoOmnibox(false));
}

TEST_F(SearchIPCRouterPolicyTest, SendSetDisplayInstantResults) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchTabHelper()->ipc_router().policy()->
      ShouldSendSetDisplayInstantResults());
}

TEST_F(SearchIPCRouterPolicyTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetSuggestionToPrefetch());
}

TEST_F(SearchIPCRouterPolicyTest,
       DoNotSendSetMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);

  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetSuggestionToPrefetch());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetDisplayInstantResults());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetPromoInformation());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest,
       AppropriateMessagesSentToIncognitoPages) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);

  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->ShouldSubmitQuery());
}

TEST_F(SearchIPCRouterPolicyTest, SendMostVisitedItems) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedItems) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, SendThemeBackgroundInfo) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendThemeBackgroundInfo) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper = GetSearchTabHelper();
  EXPECT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest, SubmitQuery) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  EXPECT_TRUE(search_tab_helper->ipc_router().policy()->ShouldSubmitQuery());
}
