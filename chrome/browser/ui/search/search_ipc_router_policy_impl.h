// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_

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
  friend class SearchIPCRouterPolicyTest;

  // Overridden from SearchIPCRouter::Policy:
  virtual bool ShouldProcessSetVoiceSearchSupport() override;
  virtual bool ShouldProcessFocusOmnibox(bool is_active_tab) override;
  virtual bool ShouldProcessNavigateToURL(bool is_active_tab) override;
  virtual bool ShouldProcessDeleteMostVisitedItem() override;
  virtual bool ShouldProcessUndoMostVisitedDeletion() override;
  virtual bool ShouldProcessUndoAllMostVisitedDeletions() override;
  virtual bool ShouldProcessLogEvent() override;
  virtual bool ShouldProcessPasteIntoOmnibox(bool is_active_tab) override;
  virtual bool ShouldProcessChromeIdentityCheck() override;
  virtual bool ShouldSendSetPromoInformation() override;
  virtual bool ShouldSendSetDisplayInstantResults() override;
  virtual bool ShouldSendSetSuggestionToPrefetch() override;
  virtual bool ShouldSendSetOmniboxStartMargin() override;
  virtual bool ShouldSendSetInputInProgress(bool is_active_tab) override;
  virtual bool ShouldSendOmniboxFocusChanged() override;
  virtual bool ShouldSendMostVisitedItems() override;
  virtual bool ShouldSendThemeBackgroundInfo() override;
  virtual bool ShouldSendToggleVoiceSearch() override;
  virtual bool ShouldSubmitQuery() override;

  // Used by unit tests.
  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

  const content::WebContents* web_contents_;
  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(SearchIPCRouterPolicyImpl);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
