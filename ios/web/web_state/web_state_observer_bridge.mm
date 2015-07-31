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

void WebStateObserverBridge::ProvisionalNavigationStarted(const GURL& url) {
  SEL selector = @selector(webState:didStartProvisionalNavigationForURL:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state() didStartProvisionalNavigationForURL:url];
  }
}

void WebStateObserverBridge::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_detatils) {
  SEL selector = @selector(webState:didCommitNavigationWithDetails:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state()
        didCommitNavigationWithDetails:load_detatils];
  }
}

void WebStateObserverBridge::DidStartLoading() {
  SEL selector = @selector(webStateDidStartLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStartLoading:web_state()];
  }
}

void WebStateObserverBridge::DidStopLoading() {
  SEL selector = @selector(webStateDidStopLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStopLoading:web_state()];
  }
}

void WebStateObserverBridge::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  SEL selector = @selector(webStateDidLoadPage:);
  if ([observer_ respondsToSelector:selector])
    [observer_ webStateDidLoadPage:web_state()];
}

void WebStateObserverBridge::InsterstitialDismissed() {
  SEL selector = @selector(webStateDidDismissInterstitial:);
  if ([observer_ respondsToSelector:selector])
    [observer_ webStateDidDismissInterstitial:web_state()];
}

void WebStateObserverBridge::UrlHashChanged() {
  SEL selector = @selector(webStateDidChangeURLHash:);
  if ([observer_ respondsToSelector:selector])
    [observer_ webStateDidChangeURLHash:web_state()];
}

void WebStateObserverBridge::HistoryStateChanged() {
  SEL selector = @selector(webStateDidChangeHistoryState:);
  if ([observer_ respondsToSelector:selector])
    [observer_ webStateDidChangeHistoryState:web_state()];
}

void WebStateObserverBridge::DocumentSubmitted(const std::string& form_name,
                                               bool user_initiated) {
  SEL selector =
      @selector(webState:didSubmitDocumentWithFormNamed:userInitiated:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state()
        didSubmitDocumentWithFormNamed:form_name
                         userInitiated:user_initiated];
  }
}

void WebStateObserverBridge::FormActivityRegistered(
    const std::string& form_name,
    const std::string& field_name,
    const std::string& type,
    const std::string& value,
    int key_code,
    bool input_missing) {
  SEL selector = @selector(webState:
      didRegisterFormActivityWithFormNamed:
                                 fieldName:
                                      type:
                                     value:
                                   keyCode:
                              inputMissing:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state()
        didRegisterFormActivityWithFormNamed:form_name
                                   fieldName:field_name
                                        type:type
                                       value:value
                                     keyCode:key_code
                                inputMissing:input_missing];
  }
}

void WebStateObserverBridge::FaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  SEL selector = @selector(webState:didUpdateFaviconURLCandidates:);
  if ([observer_ respondsToSelector:selector])
    [observer_ webState:web_state() didUpdateFaviconURLCandidates:candidates];
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
