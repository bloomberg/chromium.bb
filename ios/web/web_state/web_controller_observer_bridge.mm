// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_controller_observer_bridge.h"

#include "base/logging.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"

namespace web {

WebControllerObserverBridge::WebControllerObserverBridge(
    id<CRWWebControllerObserver> web_controller_observer,
    WebState* web_state,
    CRWWebController* web_controller)
    : WebStateObserver(web_state),
      web_controller_observer_(web_controller_observer),
      web_controller_(web_controller) {
  DCHECK(web_controller_observer_);
  DCHECK(web_controller_);
}

WebControllerObserverBridge::~WebControllerObserverBridge() {
}

void WebControllerObserverBridge::PageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == PageLoadCompletionStatus::SUCCESS &&
      [web_controller_observer_ respondsToSelector:@selector(pageLoaded:)])
    [web_controller_observer_ pageLoaded:web_controller_];
}

}  // namespace web
