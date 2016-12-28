// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/web_state_observer.h"

#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/web_state/web_state.h"

namespace web {

WebStateObserver::WebStateObserver(WebState* web_state) : web_state_(nullptr) {
  Observe(web_state);
}

WebStateObserver::WebStateObserver() : web_state_(nullptr) {}

WebStateObserver::~WebStateObserver() {
  if (web_state_)
    web_state_->RemoveObserver(this);
}

void WebStateObserver::Observe(WebState* web_state) {
  if (web_state == web_state_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (web_state_)
    web_state_->RemoveObserver(this);
  web_state_ = web_state;
  if (web_state_)
    web_state_->AddObserver(this);
}

void WebStateObserver::ResetWebState() {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

}  // namespace web
