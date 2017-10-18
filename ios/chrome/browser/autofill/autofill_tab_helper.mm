// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(AutofillTabHelper);

AutofillTabHelper::~AutofillTabHelper() = default;

// static
void AutofillTabHelper::CreateForWebState(
    web::WebState* web_state,
    password_manager::PasswordGenerationManager* password_generation_manager) {
  DCHECK(web_state);
  DCHECK(!FromWebState(web_state));
  web_state->SetUserData(UserDataKey(),
                         base::WrapUnique(new AutofillTabHelper(
                             web_state, password_generation_manager)));
}

id<FormSuggestionProvider> AutofillTabHelper::GetSuggestionProvider() {
  return controller_.suggestionProvider;
}

AutofillTabHelper::AutofillTabHelper(
    web::WebState* web_state,
    password_manager::PasswordGenerationManager* password_generation_manager)
    : web::WebStateObserver(web_state),
      controller_([[AutofillController alloc]
               initWithBrowserState:ios::ChromeBrowserState::FromBrowserState(
                                        web_state->GetBrowserState())
          passwordGenerationManager:password_generation_manager
                           webState:web_state]) {
  DCHECK(web::WebStateObserver::web_state());
}

void AutofillTabHelper::WebStateDestroyed(web::WebState* web_state) {
  [controller_ detachFromWebState];
  controller_ = nil;
}
