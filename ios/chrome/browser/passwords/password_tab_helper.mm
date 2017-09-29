// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/passwords/password_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(PasswordTabHelper);

PasswordTabHelper::~PasswordTabHelper() = default;

// static
void PasswordTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<PasswordsUiDelegate> passwords_ui_delegate) {
  DCHECK(web_state);
  DCHECK(!FromWebState(web_state));
  web_state->SetUserData(UserDataKey(), base::WrapUnique(new PasswordTabHelper(
                                            web_state, passwords_ui_delegate)));
}

void PasswordTabHelper::SetDispatcher(id<ApplicationCommands> dispatcher) {
  controller_.dispatcher = dispatcher;
}

void PasswordTabHelper::SetPasswordControllerDelegate(
    id<PasswordControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

id<FormSuggestionProvider> PasswordTabHelper::GetSuggestionProvider() {
  return controller_.suggestionProvider;
}

id<FormInputAccessoryViewProvider>
PasswordTabHelper::GetAccessoryViewProvider() {
  return controller_.accessoryViewProvider;
}

id<PasswordFormFiller> PasswordTabHelper::GetPasswordFormFiller() {
  return controller_.passwordFormFiller;
}

password_manager::PasswordGenerationManager*
PasswordTabHelper::GetPasswordGenerationManager() {
  return controller_.passwordGenerationManager;
}

PasswordTabHelper::PasswordTabHelper(
    web::WebState* web_state,
    id<PasswordsUiDelegate> passwords_ui_delegate)
    : web::WebStateObserver(web_state),
      controller_([[PasswordController alloc]
             initWithWebState:web_state
          passwordsUiDelegate:passwords_ui_delegate]) {
  DCHECK(web::WebStateObserver::web_state());
}

void PasswordTabHelper::WebStateDestroyed() {
  [controller_ detach];
  controller_ = nil;
}
