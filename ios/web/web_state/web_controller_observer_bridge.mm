// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_controller_observer_bridge.h"

#import "base/ios/crb_protocol_observers.h"
#include "base/logging.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebControllerObserverBridge::WebControllerObserverBridge(
    CRBProtocolObservers<CRWWebControllerObserver>* web_controller_observer,
    CRWWebController* web_controller)
    : web_controller_observer_(web_controller_observer),
      web_controller_(web_controller) {
  DCHECK(web_controller_observer_);
  DCHECK(web_controller_);
}

WebControllerObserverBridge::~WebControllerObserverBridge() = default;

void WebControllerObserverBridge::PageLoaded(
    WebState* web_state,
    PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == PageLoadCompletionStatus::SUCCESS) {
    [web_controller_observer_ pageLoaded:web_controller_];
  }
}

void WebControllerObserverBridge::WebStateDestroyed(WebState* web_state) {
  // WebControllerObserverBridge is owned by CRWWebController. It should be
  // unregistered from WebState observer list in CRWWebController -close,
  // the WebStateDestroyed should never be called.
  NOTREACHED();
}

}  // namespace web
