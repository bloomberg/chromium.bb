// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_web_state_observer.h"

PaymentRequestWebStateObserver::PaymentRequestWebStateObserver(
    id<PaymentRequestWebStateDelegate> delegate)
    : web::WebStateObserver(), delegate_(delegate) {}

PaymentRequestWebStateObserver::~PaymentRequestWebStateObserver() {}

void PaymentRequestWebStateObserver::ObserveWebState(web::WebState* web_state) {
  Observe(web_state);
}

void PaymentRequestWebStateObserver::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  if ([delegate_ respondsToSelector:@selector(pageLoadedWithStatus:)]) {
    [delegate_ pageLoadedWithStatus:load_completion_status];
  }
}
