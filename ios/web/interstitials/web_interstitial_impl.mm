// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/interstitials/web_interstitial_impl.h"

#include "base/logging.h"
#include "ios/web/interstitials/web_interstitial_facade_delegate.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/interstitials/web_interstitial_delegate.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
WebInterstitial* WebInterstitial::GetWebInterstitial(web::WebState* web_state) {
  return web_state->GetWebInterstitial();
}

WebInterstitialImpl::WebInterstitialImpl(WebStateImpl* web_state,
                                         bool new_navigation,
                                         const GURL& url)
    : WebStateObserver(web_state),
      navigation_manager_(&web_state->GetNavigationManagerImpl()),
      url_(url),
      facade_delegate_(nullptr),
      new_navigation_(new_navigation),
      action_taken_(false) {
  DCHECK(web_state);
}

WebInterstitialImpl::~WebInterstitialImpl() {
  Hide();
  if (facade_delegate_)
    facade_delegate_->WebInterstitialDestroyed();
}

const GURL& WebInterstitialImpl::GetUrl() const {
  return url_;
}

void WebInterstitialImpl::SetFacadeDelegate(
    WebInterstitialFacadeDelegate* facade_delegate) {
  facade_delegate_ = facade_delegate;
}

WebInterstitialFacadeDelegate* WebInterstitialImpl::GetFacadeDelegate() const {
  return facade_delegate_;
}

void WebInterstitialImpl::Show() {
  PrepareForDisplay();
  GetWebStateImpl()->ShowWebInterstitial(this);

  if (new_navigation_) {
    // TODO(stuartmorgan): Plumb transient entry handling through
    // NavigationManager, and remove the NavigationManagerImpl and
    // SessionController usage here.
    CRWSessionController* sessionController =
        navigation_manager_->GetSessionController();
    [sessionController addTransientItemWithURL:url_];

    // Give delegates a chance to set some states on the navigation item.
    GetDelegate()->OverrideItem(navigation_manager_->GetTransientItem());
  }
}

void WebInterstitialImpl::Hide() {
  GetWebStateImpl()->ClearTransientContentView();
}

void WebInterstitialImpl::DontProceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;

  // Clear the pending entry, since that's the page that's not being
  // proceeded to.
  NavigationManager* nav_manager = GetWebStateImpl()->GetNavigationManager();
  nav_manager->DiscardNonCommittedItems();

  Hide();

  GetDelegate()->OnDontProceed();

  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  if (![user_defaults boolForKey:@"PendingIndexNavigationDisabled"]) {
    // Reload last committed entry.
    nav_manager->Reload(true /* check_for_repost */);
  }

  delete this;
}

void WebInterstitialImpl::Proceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;
  Hide();
  GetDelegate()->OnProceed();
  delete this;
}

void WebInterstitialImpl::WebStateDestroyed() {
  DontProceed();
}

WebStateImpl* WebInterstitialImpl::GetWebStateImpl() const {
  return static_cast<web::WebStateImpl*>(web_state());
}

}  // namespace web
