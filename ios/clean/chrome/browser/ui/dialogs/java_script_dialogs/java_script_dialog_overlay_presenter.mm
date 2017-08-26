// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_overlay_presenter.h"

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_coordinator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(JavaScriptDialogOverlayPresenter);

// static
void JavaScriptDialogOverlayPresenter::CreateForWebState(
    web::WebState* web_state,
    OverlayService* overlay_service) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    std::unique_ptr<JavaScriptDialogOverlayPresenter> presenter =
        base::WrapUnique(
            new JavaScriptDialogOverlayPresenter(web_state, overlay_service));
    web_state->SetUserData(UserDataKey(), std::move(presenter));
  }
}

JavaScriptDialogOverlayPresenter::JavaScriptDialogOverlayPresenter(
    web::WebState* web_state,
    OverlayService* overlay_service)
    : web_state_(web_state), overlay_service_(overlay_service) {
  DCHECK(web_state_);
  DCHECK(overlay_service_);
}

JavaScriptDialogOverlayPresenter::~JavaScriptDialogOverlayPresenter() {}

void JavaScriptDialogOverlayPresenter::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    const web::DialogClosedCallback& callback) {
  // This presenter should only attempt to present dialogs from its associated
  // WebState.
  DCHECK_EQ(web_state_, web_state);
  // Create a new coordinator and add it to the overlay queue.
  JavaScriptDialogRequest* request =
      [JavaScriptDialogRequest requestWithWebState:web_state
                                              type:dialog_type
                                         originURL:origin_url
                                           message:message_text
                                 defaultPromptText:default_prompt_text
                                          callback:callback];
  overlay_service_->ShowOverlayForWebState(
      [[JavaScriptDialogCoordinator alloc] initWithRequest:request], web_state);
}

void JavaScriptDialogOverlayPresenter::CancelDialogs(web::WebState* web_state) {
  overlay_service_->CancelAllOverlaysForWebState(web_state);
}
