// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(FindTabHelper);

// static
void FindTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<FindInPageControllerDelegate> controller_delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           new FindTabHelper(web_state, controller_delegate));
  }
}

FindTabHelper::FindTabHelper(
    web::WebState* web_state,
    id<FindInPageControllerDelegate> controller_delegate) {
  controller_ =
      [[FindInPageController alloc] initWithWebState:web_state
                                            delegate:controller_delegate];
}

FindTabHelper::~FindTabHelper() {}

FindInPageController* FindTabHelper::GetController() {
  return controller_;
}

void FindTabHelper::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  [controller_ disableFindInPageWithCompletionHandler:nil];
}

void FindTabHelper::WebStateDestroyed() {
  [controller_ detachFromWebState];
}
