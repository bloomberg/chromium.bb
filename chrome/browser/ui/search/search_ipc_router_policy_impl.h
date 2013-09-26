// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/search/search_ipc_router.h"

namespace content {
class WebContents;
}

// The SearchIPCRouter::Policy implementation.
class SearchIPCRouterPolicyImpl : public SearchIPCRouter::Policy {
 public:
  explicit SearchIPCRouterPolicyImpl(const content::WebContents* web_contents);
  virtual ~SearchIPCRouterPolicyImpl();

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessVoiceSearchSupportMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           SendSetDisplayInstantResults);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSetDisplayInstantResultsForIncognitoPage);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendMostVisitedItemsForIncognitoPage);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendThemeBackgroundInfoForIncognitoPage);

  // Overridden from SearchIPCRouter::Policy:
  virtual bool ShouldProcessSetVoiceSearchSupport() OVERRIDE;
  virtual bool ShouldSendSetDisplayInstantResults() OVERRIDE;
  virtual bool ShouldSendMostVisitedItems() OVERRIDE;
  virtual bool ShouldSendThemeBackgroundInfo() OVERRIDE;

  // Used by unit tests.
  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

  const content::WebContents* web_contents_;
  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(SearchIPCRouterPolicyImpl);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
