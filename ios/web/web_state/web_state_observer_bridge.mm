// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_observer_bridge.h"

#include "base/logging.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebStateObserverBridge::WebStateObserverBridge(id<CRWWebStateObserver> observer)
    : observer_(observer) {}

WebStateObserverBridge::WebStateObserverBridge(web::WebState* web_state,
                                               id<CRWWebStateObserver> observer)
    : web_state_(web_state), observer_(observer) {
  if (web_state_) {
    web_state_->AddObserver(this);
  }
}

WebStateObserverBridge::~WebStateObserverBridge() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void WebStateObserverBridge::WasShown(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webStateWasShown:)]) {
    [observer_ webStateWasShown:web_state];
  }
}

void WebStateObserverBridge::WasHidden(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webStateWasHidden:)]) {
    [observer_ webStateWasHidden:web_state];
  }
}

void WebStateObserverBridge::NavigationItemsPruned(web::WebState* web_state,
                                                   size_t pruned_item_count) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:didPruneNavigationItemsWithCount:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state
        didPruneNavigationItemsWithCount:pruned_item_count];
  }
}

void WebStateObserverBridge::NavigationItemCommitted(
    web::WebState* web_state,
    const web::LoadCommittedDetails& load_detatils) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:didCommitNavigationWithDetails:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state didCommitNavigationWithDetails:load_detatils];
  }
}

void WebStateObserverBridge::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webState:didStartNavigation:)]) {
    [observer_ webState:web_state didStartNavigation:navigation_context];
  }
}

void WebStateObserverBridge::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webState:didFinishNavigation:)]) {
    [observer_ webState:web_state didFinishNavigation:navigation_context];
  }
}

void WebStateObserverBridge::DidStartLoading(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webStateDidStartLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStartLoading:web_state];
  }
}

void WebStateObserverBridge::DidStopLoading(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webStateDidStopLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStopLoading:web_state];
  }
}

void WebStateObserverBridge::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:didLoadPageWithSuccess:);
  if ([observer_ respondsToSelector:selector]) {
    BOOL success = NO;
    switch (load_completion_status) {
      case PageLoadCompletionStatus::SUCCESS:
        success = YES;
        break;
      case PageLoadCompletionStatus::FAILURE:
        success = NO;
        break;
    }
    [observer_ webState:web_state didLoadPageWithSuccess:success];
  }
}

void WebStateObserverBridge::LoadProgressChanged(web::WebState* web_state,
                                                 double progress) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:didChangeLoadingProgress:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state didChangeLoadingProgress:progress];
  }
}

void WebStateObserverBridge::TitleWasSet(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webStateDidChangeTitle:)]) {
    [observer_ webStateDidChangeTitle:web_state];
  }
}

void WebStateObserverBridge::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webStateDidChangeVisibleSecurityState:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidChangeVisibleSecurityState:web_state];
  }
}

void WebStateObserverBridge::DidSuppressDialog(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(webStateDidSuppressDialog:)]) {
    [observer_ webStateDidSuppressDialog:web_state];
  }
}

void WebStateObserverBridge::DocumentSubmitted(web::WebState* web_state,
                                               const std::string& form_name,
                                               bool user_initiated) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector =
      @selector(webState:didSubmitDocumentWithFormNamed:userInitiated:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state
        didSubmitDocumentWithFormNamed:form_name
                         userInitiated:user_initiated];
  }
}

void WebStateObserverBridge::FormActivityRegistered(
    web::WebState* web_state,
    const std::string& form_name,
    const std::string& field_name,
    const std::string& type,
    const std::string& value,
    bool input_missing) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:
      didRegisterFormActivityWithFormNamed:
                                 fieldName:
                                      type:
                                     value:
                              inputMissing:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state
        didRegisterFormActivityWithFormNamed:form_name
                                   fieldName:field_name
                                        type:type
                                       value:value
                                inputMissing:input_missing];
  }
}

void WebStateObserverBridge::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<FaviconURL>& candidates) {
  DCHECK(!web_state_ || web_state_ == web_state);
  SEL selector = @selector(webState:didUpdateFaviconURLCandidates:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state didUpdateFaviconURLCandidates:candidates];
  }
}

void WebStateObserverBridge::RenderProcessGone(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if ([observer_ respondsToSelector:@selector(renderProcessGoneForWebState:)]) {
    [observer_ renderProcessGoneForWebState:web_state];
  }
}

void WebStateObserverBridge::WebStateDestroyed(web::WebState* web_state) {
  DCHECK(!web_state_ || web_state_ == web_state);
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }

  SEL selector = @selector(webStateDestroyed:);
  if ([observer_ respondsToSelector:selector]) {
    // |webStateDestroyed:| may delete |this|, so don't expect |this| to be
    // valid afterwards.
    [observer_ webStateDestroyed:web_state];
  }
}

void WebStateObserverBridge::Observe(WebState* web_state) {
  // No class should sub-class WebStateObserverBridge, so this method should
  // never be called. It is there to protect a potential sub-class from calling
  // WebStateObserver implementation (this would break WebStateObserverBridge).
  NOTREACHED();
}

}  // namespace web
