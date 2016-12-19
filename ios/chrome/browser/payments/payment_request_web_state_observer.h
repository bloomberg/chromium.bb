// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_WEB_STATE_OBSERVER_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_state/web_state_observer.h"

@protocol PaymentRequestWebStateDelegate<NSObject>
@optional
- (void)pageLoadedWithStatus:(web::PageLoadCompletionStatus)loadStatus;

@end

class PaymentRequestWebStateObserver : public web::WebStateObserver {
 public:
  explicit PaymentRequestWebStateObserver(
      id<PaymentRequestWebStateDelegate> delegate);
  ~PaymentRequestWebStateObserver() override;

  void ObserveWebState(web::WebState* web_state);

 private:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;

  id<PaymentRequestWebStateDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_WEB_STATE_OBSERVER_H_
