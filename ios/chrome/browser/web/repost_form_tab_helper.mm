// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/repost_form_tab_helper.h"

#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(RepostFormTabHelper);

RepostFormTabHelper::RepostFormTabHelper(
    web::WebState* web_state,
    id<RepostFormTabHelperDelegate> delegate)
    : web::WebStateObserver(web_state), delegate_(delegate) {}

RepostFormTabHelper::~RepostFormTabHelper() = default;

void RepostFormTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<RepostFormTabHelperDelegate> delegate) {
  DCHECK(web_state);
  DCHECK(delegate);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new RepostFormTabHelper(web_state, delegate)));
  }
}

void RepostFormTabHelper::PresentDialog(
    CGPoint location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!is_presenting_dialog_);
  is_presenting_dialog_ = true;
  base::Callback<void(bool)> local_callback(callback);
  [delegate_ repostFormTabHelper:this
      presentRepostFromDialogAtPoint:location
                   completionHandler:^(BOOL should_continue) {
                     is_presenting_dialog_ = false;
                     local_callback.Run(should_continue);
                   }];
}

void RepostFormTabHelper::DidStartNavigation(web::NavigationContext*) {
  if (is_presenting_dialog_)
    [delegate_ repostFormTabHelperDismissRepostFormDialog:this];
  is_presenting_dialog_ = false;
}

void RepostFormTabHelper::WebStateDestroyed() {
  if (is_presenting_dialog_)
    [delegate_ repostFormTabHelperDismissRepostFormDialog:this];
  is_presenting_dialog_ = false;
}
