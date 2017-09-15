// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_overlay_presenter.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_coordinator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_BROWSER_USER_DATA_KEY(JavaScriptDialogOverlayPresenter);

JavaScriptDialogOverlayPresenter::JavaScriptDialogOverlayPresenter(
    Browser* browser)
    : overlay_service_(OverlayServiceFactory::GetInstance()->GetForBrowserState(
          browser->browser_state())) {}

JavaScriptDialogOverlayPresenter::~JavaScriptDialogOverlayPresenter() {}

void JavaScriptDialogOverlayPresenter::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    const web::DialogClosedCallback& callback) {
  // Block the dialog if instructed by the blocking state.
  JavaScriptDialogBlockingState* blocking_state =
      JavaScriptDialogBlockingState::FromWebState(web_state);
  if (blocking_state && blocking_state->blocked()) {
    callback.Run(NO, nil);
    return;
  }
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
