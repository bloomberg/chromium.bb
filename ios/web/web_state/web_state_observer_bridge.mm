// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_observer_bridge.h"

namespace web {

WebStateObserverBridge::WebStateObserverBridge(web::WebState* webState,
                                               id<CRWWebStateObserver> observer)
    : web::WebStateObserver(webState), observer_(observer) {
}

WebStateObserverBridge::~WebStateObserverBridge() {
}

void WebStateObserverBridge::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  SEL selector = @selector(pageLoaded:);
  if ([observer_ respondsToSelector:selector])
    [observer_ pageLoaded:web_state()];
}

void WebStateObserverBridge::DocumentSubmitted(
    const std::string& form_name, bool user_interaction) {
  SEL selector = @selector(documentSubmitted:formName:userInteraction:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ documentSubmitted:web_state()
                        formName:form_name
                 userInteraction:user_interaction];
  }
}

void WebStateObserverBridge::FormActivityRegistered(
    const std::string& form_name,
    const std::string& field_name,
    const std::string& type,
    const std::string& value,
    int key_code,
    bool error) {
  SEL selector =
      @selector(formActivity:formName:fieldName:type:value:keyCode:error:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ formActivity:web_state()
                   formName:form_name
                  fieldName:field_name
                       type:type
                      value:value
                    keyCode:key_code
                      error:error];
  }
}

void WebStateObserverBridge::WebStateDestroyed() {
  SEL selector = @selector(webStateDestroyed:);
  if ([observer_ respondsToSelector:selector]) {
    // |webStateDestroyed:| may delete |this|, so don't expect |this| to be
    // valid afterwards.
    [observer_ webStateDestroyed:web_state()];
  }
}

}  // namespace web
