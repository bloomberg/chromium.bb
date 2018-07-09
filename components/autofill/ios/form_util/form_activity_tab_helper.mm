// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/values.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "ios/web/public/web_state/form_activity_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(autofill::FormActivityTabHelper);

namespace autofill {

FormActivityTabHelper::~FormActivityTabHelper() = default;

// static
FormActivityTabHelper* FormActivityTabHelper::GetOrCreateForWebState(
    web::WebState* web_state) {
  FormActivityTabHelper* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

FormActivityTabHelper::FormActivityTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

void FormActivityTabHelper::AddObserver(FormActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void FormActivityTabHelper::RemoveObserver(FormActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FormActivityTabHelper::FormActivityRegistered(
    web::WebState* web_state,
    const web::FormActivityParams& params) {
  for (auto& observer : observers_)
    observer.OnFormActivity(web_state, params);
}

void FormActivityTabHelper::DocumentSubmitted(web::WebState* web_state,
                                              const std::string& form_name,
                                              bool user_initiated,
                                              bool is_main_frame) {
  for (auto& observer : observers_)
    observer.DidSubmitDocument(web_state, form_name, user_initiated,
                               is_main_frame);
}

void FormActivityTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

}  // namespace autofill
