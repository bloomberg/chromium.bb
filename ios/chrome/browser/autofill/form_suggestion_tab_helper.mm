// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(FormSuggestionTabHelper);

FormSuggestionTabHelper::~FormSuggestionTabHelper() = default;

// static
void FormSuggestionTabHelper::CreateForWebState(
    web::WebState* web_state,
    NSArray<id<FormSuggestionProvider>>* providers) {
  DCHECK(web_state);
  DCHECK(!FromWebState(web_state));
  web_state->SetUserData(
      UserDataKey(),
      base::WrapUnique(new FormSuggestionTabHelper(web_state, providers)));
}

id<FormInputAccessoryViewProvider>
FormSuggestionTabHelper::GetAccessoryViewProvider() {
  return controller_.accessoryViewProvider;
}

FormSuggestionTabHelper::FormSuggestionTabHelper(
    web::WebState* web_state,
    NSArray<id<FormSuggestionProvider>>* providers)
    : web::WebStateObserver(web_state),
      controller_([[FormSuggestionController alloc]
          initWithWebState:web_state
                 providers:providers]) {
  DCHECK(web::WebStateObserver::web_state());
}

void FormSuggestionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  [controller_ detachFromWebState];
  controller_ = nil;
}
