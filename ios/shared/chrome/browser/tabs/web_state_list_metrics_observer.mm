// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list_metrics_observer.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListMetricsObserver::WebStateListMetricsObserver() = default;

WebStateListMetricsObserver::~WebStateListMetricsObserver() = default;

void WebStateListMetricsObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
}

void WebStateListMetricsObserver::WebStateMoved(WebStateList* web_state_list,
                                                web::WebState* web_state,
                                                int from_index,
                                                int to_index) {}

void WebStateListMetricsObserver::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  // Record a tab clobber, since swapping tabs bypasses the Tab code that would
  // normally log clobbers.
  base::RecordAction(base::UserMetricsAction("MobileTabClobbered"));
}

void WebStateListMetricsObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
}
