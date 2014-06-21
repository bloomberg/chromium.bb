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
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SearchIPCRouter::Policy* GetSearchIPCRouterPolicy() {
    SearchTabHelper* search_tab_helper =
        SearchTabHelper::FromWebContents(web_contents());
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return search_tab_helper->ipc_router().policy_for_testing();
  }

  void SetIncognitoProfile() {
    SearchIPCRouterPolicyImpl* policy =
        static_cast<SearchIPCRouterPolicyImpl*>(GetSearchIPCRouterPolicy());
    policy->set_is_incognito(true);
  }
};

TEST_F(SearchIPCRouterPolicyTest, ProcessVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->
      ShouldProcessSetVoiceSearchSupport());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessFocusOmnibox) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessFocusOmnibox) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessFocusOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, SendSetPromoInformation) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendSetPromoInformation());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendSetPromoInformation) {
  // Send promo information only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendSetPromoInformation());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessDeleteMostVisitedItem) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessDeleteMostVisitedItem());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoMostVisitedDeletion) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->
      ShouldProcessUndoMostVisitedDeletion());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessUndoAllMostVisitedDeletions) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->
      ShouldProcessUndoAllMostVisitedDeletions());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessLogEvent) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessLogEvent) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessLogEvent());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessChromeIdentityCheck) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessChromeIdentityCheck());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessChromeIdentityCheck) {
  // Process message only if the underlying page is an InstantNTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessChromeIdentityCheck());
}

TEST_F(SearchIPCRouterPolicyTest, ProcessNavigateToURL) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessNavigateToURL(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessNavigateToURL) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessNavigateToURL(true));

  NavigateAndCommitActiveTab(GURL("file://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessNavigateToURL(true));
}

TEST_F(SearchIPCRouterPolicyTest, ProcessPasteIntoOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessPasteIntoOmniboxMsg) {
  // Process message only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetIncognitoProfile();

  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldProcessFocusOmnibox(true));
  EXPECT_FALSE(router_policy->ShouldProcessNavigateToURL(true));
  EXPECT_FALSE(router_policy->ShouldProcessDeleteMostVisitedItem());
  EXPECT_FALSE(router_policy->ShouldProcessUndoMostVisitedDeletion());
  EXPECT_FALSE(router_policy->ShouldProcessUndoAllMostVisitedDeletions());
  EXPECT_FALSE(router_policy->ShouldProcessLogEvent());
  EXPECT_FALSE(router_policy->ShouldProcessPasteIntoOmnibox(true));
}

TEST_F(SearchIPCRouterPolicyTest, DoNotProcessMessagesForInactiveTab) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Assume the NTP is deactivated.
  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldProcessFocusOmnibox(false));
  EXPECT_FALSE(router_policy->ShouldProcessNavigateToURL(false));
  EXPECT_FALSE(router_policy->ShouldProcessPasteIntoOmnibox(false));
  EXPECT_FALSE(router_policy->ShouldSendSetInputInProgress(false));
}

TEST_F(SearchIPCRouterPolicyTest, SendSetDisplayInstantResults) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendSetDisplayInstantResults());
}

TEST_F(SearchIPCRouterPolicyTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendSetSuggestionToPrefetch());
}

TEST_F(SearchIPCRouterPolicyTest, SendSetOmniboxStartMargin) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendSetOmniboxStartMargin());
}

TEST_F(SearchIPCRouterPolicyTest,
       DoNotSendSetMessagesForIncognitoPage) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetIncognitoProfile();

  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_FALSE(router_policy->ShouldSendSetSuggestionToPrefetch());
  EXPECT_FALSE(router_policy->ShouldSendSetDisplayInstantResults());
  EXPECT_FALSE(router_policy->ShouldSendSetPromoInformation());
  EXPECT_FALSE(router_policy->ShouldSendThemeBackgroundInfo());
  EXPECT_FALSE(router_policy->ShouldSendMostVisitedItems());
  EXPECT_FALSE(router_policy->ShouldSendSetInputInProgress(true));
  EXPECT_FALSE(router_policy->ShouldSendOmniboxFocusChanged());
}

TEST_F(SearchIPCRouterPolicyTest,
       AppropriateMessagesSentToIncognitoPages) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetIncognitoProfile();

  SearchIPCRouter::Policy* router_policy = GetSearchIPCRouterPolicy();
  EXPECT_TRUE(router_policy->ShouldSubmitQuery());
  EXPECT_TRUE(router_policy->ShouldSendToggleVoiceSearch());
  EXPECT_TRUE(router_policy->ShouldSendSetOmniboxStartMargin());
}

TEST_F(SearchIPCRouterPolicyTest, SendMostVisitedItems) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedItems) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, SendThemeBackgroundInfo) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendThemeBackgroundInfo) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_FALSE(GetSearchIPCRouterPolicy()->ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest, SubmitQuery) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSubmitQuery());
}

TEST_F(SearchIPCRouterPolicyTest, SendToggleVoiceSearch) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(GetSearchIPCRouterPolicy()->ShouldSendToggleVoiceSearch());
}
