// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_MEDIATOR_H_

#include "ios/chrome/browser/ui/payments/payment_request_view_controller.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// A mediator object that provides data for a PaymentRequestViewController.
@interface PaymentRequestMediator
    : NSObject<PaymentRequestViewControllerDataSource>

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_MEDIATOR_H
