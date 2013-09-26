// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "base/command_line.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SearchIPCRouterPolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableInstantExtendedAPI);
    ChromeRenderViewHostTestHarness::SetUp();
    SearchTabHelper::CreateForWebContents(web_contents());
  }
};

TEST_F(SearchIPCRouterPolicyTest, ProcessVoiceSearchSupportMsg) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldProcessSetVoiceSearchSupport());
}

TEST_F(SearchIPCRouterPolicyTest, SendSetDisplayInstantResults) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetDisplayInstantResults());
}

TEST_F(SearchIPCRouterPolicyTest,
       DoNotSetDisplayInstantResultsForIncognitoPage) {
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);
  ASSERT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendSetDisplayInstantResults());
}

TEST_F(SearchIPCRouterPolicyTest, SendMostVisitedItems) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedItems) {
  // Send most visited items only if the current tab is an Instant NTP.
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendMostVisitedItemsForIncognitoPage) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);
  ASSERT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendMostVisitedItems());
}

TEST_F(SearchIPCRouterPolicyTest, SendThemeBackgroundInfo) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_TRUE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest, DoNotSendThemeBackgroundInfo) {
  // Send theme background information only if the current tab is an
  // Instant NTP.
  NavigateAndCommit(GURL("chrome-search://foo/bar"));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  ASSERT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterPolicyTest,
       DoNotSendThemeBackgroundInfoForIncognitoPage) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
  SearchIPCRouterPolicyImpl* policy =
      static_cast<SearchIPCRouterPolicyImpl*>(
          search_tab_helper->ipc_router().policy());
  policy->set_is_incognito(true);
  ASSERT_FALSE(search_tab_helper->ipc_router().policy()->
      ShouldSendThemeBackgroundInfo());
}
