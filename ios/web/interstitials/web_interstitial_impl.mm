// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/interstitials/web_interstitial_impl.h"

#include "base/logging.h"
#include "ios/web/interstitials/web_interstitial_facade_delegate.h"
#include "ios/web/public/interstitials/web_interstitial_delegate.h"
#include "ios/web/web_state/web_state_impl.h"

namespace web {

// static
WebInterstitial* WebInterstitial::GetWebInterstitial(web::WebState* web_state) {
  return web_state->GetWebInterstitial();
}

WebInterstitialImpl::WebInterstitialImpl(WebStateImpl* web_state,
                                         const GURL& url)
    : WebStateObserver(web_state),
      url_(url),
      facade_delegate_(nullptr),
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
}

void WebInterstitialImpl::Hide() {
  GetWebStateImpl()->ClearTransientContentView();
}

void WebInterstitialImpl::DontProceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;
  Hide();
  // Clean up unsafe nav items.
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

void WebInterstitialImpl::WebStateDestroyed() {
  DontProceed();
}

WebStateImpl* WebInterstitialImpl::GetWebStateImpl() const {
  return static_cast<web::WebStateImpl*>(web_state());
}

}  // namespace web
