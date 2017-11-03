// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/interstitials/web_interstitial_impl.h"

#include "base/logging.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/interstitials/web_interstitial_delegate.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/reload_type.h"
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
      new_navigation_(new_navigation),
      action_taken_(false) {
  DCHECK(web_state);
}

WebInterstitialImpl::~WebInterstitialImpl() {
  Hide();
}

const GURL& WebInterstitialImpl::GetUrl() const {
  return url_;
}

void WebInterstitialImpl::Show() {
  PrepareForDisplay();
  GetWebStateImpl()->ShowWebInterstitial(this);

  if (new_navigation_) {
    // TODO(crbug.com/706578): Plumb transient entry handling through
    // NavigationManager, and remove the NavigationManagerImpl usage here.
    navigation_manager_->AddTransientItem(url_);

    // Give delegates a chance to set some states on the navigation item.
    GetDelegate()->OverrideItem(navigation_manager_->GetTransientItem());
  }
}

void WebInterstitialImpl::Hide() {
  GetWebStateImpl()->ClearTransientContent();
}

void WebInterstitialImpl::DontProceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;

  // Clear the pending entry, since that's the page that's not being
  // proceeded to.
  GetWebStateImpl()->GetNavigationManager()->DiscardNonCommittedItems();

  Hide();

  GetDelegate()->OnDontProceed();

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

void WebInterstitialImpl::WebStateDestroyed(WebState* web_state) {
  DontProceed();
}

WebStateImpl* WebInterstitialImpl::GetWebStateImpl() const {
  return static_cast<web::WebStateImpl*>(web_state());
}

}  // namespace web
