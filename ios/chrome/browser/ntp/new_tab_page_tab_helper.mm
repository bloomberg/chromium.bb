// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Internally the NTP URL is about://newtab/.  However, with
// |url::kAboutScheme|, there's no host value, only a path.  Use this value for
// matching the NTP.
const char kAboutNewTabPath[] = "//newtab/";
}  // namespace

// static
void NewTabPageTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<NewTabPageTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new NewTabPageTabHelper(web_state, delegate)));
  }
}

NewTabPageTabHelper::~NewTabPageTabHelper() = default;

NewTabPageTabHelper::NewTabPageTabHelper(
    web::WebState* web_state,
    id<NewTabPageTabHelperDelegate> delegate)
    : delegate_(delegate), web_state_(web_state) {
  DCHECK(delegate);
  DCHECK(base::FeatureList::IsEnabled(kBrowserContainerContainsNTP));

  web_state->AddObserver(this);

  active_ = IsNTPURL(web_state->GetVisibleURL());
  if (active_) {
    UpdatePendingItem();
    [delegate_ newTabPageHelperDidChangeVisibility:this forWebState:web_state_];
  }
}

bool NewTabPageTabHelper::IsActive() const {
  return active_;
}

void NewTabPageTabHelper::Deactivate() {
  SetActive(false);
}

#pragma mark - WebStateObserver

void NewTabPageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  SetActive(false);
}

void NewTabPageTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsNTPURL(navigation_context->GetUrl())) {
    UpdatePendingItem();
  } else {
    SetActive(false);
  }
}

void NewTabPageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  SetActive(IsNTPURL(web_state->GetLastCommittedURL()));
}

#pragma mark - Private

void NewTabPageTabHelper::SetActive(bool active) {
  bool was_active = active_;
  active_ = active;

  // Tell |delegate_| to show or hide the NTP, if necessary.
  if (active_ != was_active) {
    [delegate_ newTabPageHelperDidChangeVisibility:this forWebState:web_state_];
  }
}

void NewTabPageTabHelper::UpdatePendingItem() {
  web::NavigationManager* manager = web_state_->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();
  if (item) {
    item->SetVirtualURL(GURL(kChromeUINewTabURL));
    item->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

bool NewTabPageTabHelper::IsNTPURL(const GURL& url) {
  // |url| can be chrome://newtab/ or about://newtab/ depending on where |url|
  // comes from (the VisibleURL chrome:// from a navigation item or the actual
  // webView url about://).  If the url is about://newtab/, there is no origin
  // to match, so instead check the scheme and the path.
  return url.GetOrigin() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) && url.path() == kAboutNewTabPath);
}
