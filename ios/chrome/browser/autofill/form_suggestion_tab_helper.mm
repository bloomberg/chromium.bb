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

FormSuggestionTabHelper::~FormSuggestionTabHelper() = default;

// static
void FormSuggestionTabHelper::CreateForWebState(
    web::WebState* web_state,
    NSArray<id<FormSuggestionProvider>>* providers) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new FormSuggestionTabHelper(web_state, providers)));
  }
}

id<FormInputSuggestionsProvider>
FormSuggestionTabHelper::GetAccessoryViewProvider() {
  return controller_;
}

FormSuggestionTabHelper::FormSuggestionTabHelper(
    web::WebState* web_state,
    NSArray<id<FormSuggestionProvider>>* providers)
    : controller_([[FormSuggestionController alloc]
          initWithWebState:web_state
                 providers:providers]) {
  web_state->AddObserver(this);
}

void FormSuggestionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  [controller_ detachFromWebState];
  web_state->RemoveObserver(this);
  controller_ = nil;
}

WEB_STATE_USER_DATA_KEY_IMPL(FormSuggestionTabHelper)
