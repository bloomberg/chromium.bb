// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/dialogs/java_script_dialog_presenter_impl.h"

#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"
#import "ios/chrome/browser/ui/dialogs/javascript_dialog_blocking_util.h"

JavaScriptDialogPresenterImpl::JavaScriptDialogPresenterImpl(
    DialogPresenter* dialogPresenter)
    : dialog_presenter_([dialogPresenter retain]) {}

JavaScriptDialogPresenterImpl::~JavaScriptDialogPresenterImpl() {}

void JavaScriptDialogPresenterImpl::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    const web::DialogClosedCallback& callback) {
  if (ShouldBlockJavaScriptDialogs(web_state)) {
    // Block the dialog if needed.
    callback.Run(NO, nil);
    return;
  }
  switch (dialog_type) {
    case web::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      web::DialogClosedCallback scoped_callback = callback;
      [dialog_presenter_ runJavaScriptAlertPanelWithMessage:message_text
                                                 requestURL:origin_url
                                                   webState:web_state
                                          completionHandler:^{
                                            if (!scoped_callback.is_null()) {
                                              scoped_callback.Run(YES, nil);
                                            }
                                          }];
      break;
    }
    case web::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      web::DialogClosedCallback scoped_callback = callback;
      [dialog_presenter_
          runJavaScriptConfirmPanelWithMessage:message_text
                                    requestURL:origin_url
                                      webState:web_state
                             completionHandler:^(BOOL is_confirmed) {
                               if (!scoped_callback.is_null()) {
                                 scoped_callback.Run(is_confirmed, nil);
                               }
                             }];
      break;
    }
    case web::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      web::DialogClosedCallback scoped_callback = callback;
      [dialog_presenter_
          runJavaScriptTextInputPanelWithPrompt:message_text
                                    defaultText:default_prompt_text
                                     requestURL:origin_url
                                       webState:web_state
                              completionHandler:^(NSString* text_input) {
                                if (!scoped_callback.is_null()) {
                                  scoped_callback.Run(YES, text_input);
                                }
                              }];
      break;
    }
    default:
      break;
  }
}

void JavaScriptDialogPresenterImpl::CancelDialogs(web::WebState* web_state) {
  [dialog_presenter_ cancelDialogForWebState:web_state];
}
