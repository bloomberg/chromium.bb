// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_observer_bridge.h"

#include "base/logging.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
FormActivityObserverBridge::FormActivityObserverBridge(
    web::WebState* web_state,
    id<FormActivityObserver> owner)
    : web_state_(web_state), owner_(owner) {
  FormActivityTabHelper::GetOrCreateForWebState(web_state)->AddObserver(this);
}

FormActivityObserverBridge::~FormActivityObserverBridge() {
  FormActivityTabHelper::GetOrCreateForWebState(web_state_)
      ->RemoveObserver(this);
}

void FormActivityObserverBridge::FormActivityRegistered(
    web::WebState* web_state,
    const FormActivityParams& params) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_
          respondsToSelector:@selector(webState:didRegisterFormActivity:)]) {
    [owner_ webState:web_state didRegisterFormActivity:params];
  }
}

void FormActivityObserverBridge::DocumentSubmitted(web::WebState* web_state,
                                                   const std::string& form_name,
                                                   bool has_user_gesture,
                                                   bool form_in_main_frame) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_ respondsToSelector:@selector
              (webState:didSubmitDocumentWithFormNamed:hasUserGesture
                          :formInMainFrame:)]) {
    [owner_ webState:web_state
        didSubmitDocumentWithFormNamed:form_name
                        hasUserGesture:has_user_gesture
                       formInMainFrame:form_in_main_frame];
  }
}
}  // namespace autofill
