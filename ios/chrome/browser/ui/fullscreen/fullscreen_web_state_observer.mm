// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns whether |web_state|'s last committed NavigationItem has a broken SSL.
bool IsWebStateSSLBroken(web::WebState* web_state) {
  if (!web_state)
    return false;
  web::NavigationManager* manager = web_state->GetNavigationManager();
  if (!manager)
    return false;
  web::NavigationItem* item = manager->GetLastCommittedItem();
  if (!item)
    return false;
  const web::SSLStatus& ssl = item->GetSSL();
  return ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
}
}  // namespace

FullscreenWebStateObserver::FullscreenWebStateObserver(FullscreenModel* model)
    : model_(model) {
  DCHECK(model_);
}

void FullscreenWebStateObserver::SetWebState(web::WebState* web_state) {
  if (web_state_ == web_state)
    return;
  if (web_state_)
    web_state_->RemoveObserver(this);
  web_state_ = web_state;
  if (web_state_)
    web_state_->AddObserver(this);
  // Update the model according to the new WebState.
  SetIsLoading(web_state ? web_state->IsLoading() : false);
  SetIsSSLBroken(web_state ? IsWebStateSSLBroken(web_state) : false);
}

void FullscreenWebStateObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  model_->ResetForNavigation();
}

void FullscreenWebStateObserver::DidStartLoading(web::WebState* web_state) {
  SetIsLoading(true);
}

void FullscreenWebStateObserver::DidStopLoading(web::WebState* web_state) {
  SetIsLoading(false);
}

void FullscreenWebStateObserver::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  SetIsSSLBroken(IsWebStateSSLBroken(web_state));
}

void FullscreenWebStateObserver::SetIsSSLBroken(bool broken) {
  if (ssl_broken_ == broken)
    return;
  ssl_broken_ = broken;
  // Fullscreen should be disbaled for pages with broken SSL.
  if (ssl_broken_)
    model_->IncrementDisabledCounter();
  else
    model_->DecrementDisabledCounter();
}

void FullscreenWebStateObserver::SetIsLoading(bool loading) {
  if (loading_ == loading)
    return;
  loading_ = loading;
  // Fullscreen should be disabled while the web view is loading.
  if (loading_)
    model_->IncrementDisabledCounter();
  else
    model_->DecrementDisabledCounter();
}
