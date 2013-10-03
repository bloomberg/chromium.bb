// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if |web_contents| corresponds to the current active tab.
bool IsActiveWebContents(const content::WebContents* web_contents) {
  Browser* browser = NULL;

// iOS and Android doesn't use the Instant framework.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  browser = chrome::FindBrowserWithWebContents(web_contents);
#endif
  return browser && web_contents &&
      (browser->tab_strip_model()->GetActiveWebContents() == web_contents);
}

}  // namespace

SearchIPCRouterPolicyImpl::SearchIPCRouterPolicyImpl(
    const content::WebContents* web_contents)
    : web_contents_(web_contents),
      is_incognito_(true) {
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile)
    is_incognito_ = profile->IsOffTheRecord();
}

SearchIPCRouterPolicyImpl::~SearchIPCRouterPolicyImpl() {}

bool SearchIPCRouterPolicyImpl::ShouldProcessSetVoiceSearchSupport() {
  return true;
}

bool SearchIPCRouterPolicyImpl::ShouldSendSetPromoInformation() {
  return !is_incognito_ && chrome::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldSendSetDisplayInstantResults() {
  return !is_incognito_;
}

bool SearchIPCRouterPolicyImpl::ShouldSendSetSuggestionToPrefetch() {
  return !is_incognito_;
}

bool SearchIPCRouterPolicyImpl::ShouldSendMostVisitedItems() {
  return !is_incognito_ && chrome::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldSendThemeBackgroundInfo() {
  return !is_incognito_ && chrome::IsInstantNTP(web_contents_);
}
