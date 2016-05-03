// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate.h"

#import "ios/web/public/web_state/web_state.h"

namespace web {

WebStateDelegate::WebStateDelegate() {}

WebStateDelegate::~WebStateDelegate() {
  while (!attached_states_.empty()) {
    WebState* web_state = *attached_states_.begin();
    web_state->SetDelegate(nullptr);
  }
  DCHECK(attached_states_.empty());
}

void WebStateDelegate::LoadProgressChanged(WebState*, double) {}

bool WebStateDelegate::HandleContextMenu(WebState*, const ContextMenuParams&) {
  return false;
}

void WebStateDelegate::Attach(WebState* source) {
  DCHECK(attached_states_.find(source) == attached_states_.end());
  attached_states_.insert(source);
}

void WebStateDelegate::Detach(WebState* source) {
  DCHECK(attached_states_.find(source) != attached_states_.end());
  attached_states_.erase(source);
}

}  // web
