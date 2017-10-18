// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(JavaScriptDialogBlockingState);

JavaScriptDialogBlockingState::JavaScriptDialogBlockingState(
    web::WebState* web_state)
    : web::WebStateObserver(web_state), dialog_count_(0), blocked_(false) {
  DCHECK(web_state);
}

JavaScriptDialogBlockingState::~JavaScriptDialogBlockingState() {
  // It is expected that WebStateDestroyed() will be received before this state
  // is deallocated.
  DCHECK(!web_state());
}

void JavaScriptDialogBlockingState::NavigationItemCommitted(
    web::WebState* web_state,
    const web::LoadCommittedDetails& load_details) {
  dialog_count_ = 0;
  blocked_ = false;
}

void JavaScriptDialogBlockingState::WebStateDestroyed(
    web::WebState* web_state) {
  Observe(nullptr);
}
