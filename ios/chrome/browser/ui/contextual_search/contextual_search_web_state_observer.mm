// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/contextual_search/contextual_search_web_state_observer.h"

ContextualSearchWebStateObserver::ContextualSearchWebStateObserver(
    id<ContextualSearchWebStateDelegate> delegate)
    : web::WebStateObserver(), delegate_(delegate), loaded_(false) {}

ContextualSearchWebStateObserver::~ContextualSearchWebStateObserver() {}

void ContextualSearchWebStateObserver::ObserveWebState(
    web::WebState* web_state) {
  loaded_ = false;
  Observe(web_state);
}

void ContextualSearchWebStateObserver::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  if (loaded_ && [delegate_ respondsToSelector:@selector(webState:
                                                   navigatedWithDetails:)]) {
    [delegate_ webState:web_state() navigatedWithDetails:load_details];
  }
}

void ContextualSearchWebStateObserver::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  if ([delegate_
          respondsToSelector:@selector(webState:pageLoadedWithStatus:)]) {
    [delegate_ webState:web_state()
        pageLoadedWithStatus:load_completion_status];
  }
  loaded_ = true;
}

void ContextualSearchWebStateObserver::WebStateDestroyed() {
  if ([delegate_ respondsToSelector:@selector(webStateDestroyed:)]) {
    [delegate_ webStateDestroyed:web_state()];
  }
}
