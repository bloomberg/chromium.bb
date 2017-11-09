// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(JavaScriptDialogBlockingState);

JavaScriptDialogBlockingState::JavaScriptDialogBlockingState(
    web::WebState* web_state)
    : web::WebStateObserver(web_state) {
  DCHECK(web_state);
}

JavaScriptDialogBlockingState::~JavaScriptDialogBlockingState() {
  // It is expected that WebStateDestroyed() will be received before this state
  // is deallocated.
  DCHECK(!web_state());
}

void JavaScriptDialogBlockingState::JavaScriptDialogBlockingOptionSelected() {
  blocked_item_ = web_state()->GetNavigationManager()->GetLastCommittedItem();
  DCHECK(blocked_item_);
}

void JavaScriptDialogBlockingState::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  // The dialog blocking state should be reset for user-initiated loads or for
  // document-changing, non-reload navigations.
  if (!navigation_context->IsRendererInitiated() ||
      (!navigation_context->IsSameDocument() && item != blocked_item_)) {
    dialog_count_ = 0;
    blocked_item_ = nullptr;
  }
}

void JavaScriptDialogBlockingState::WebStateDestroyed(
    web::WebState* web_state) {
  Observe(nullptr);
}
