// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_data_source.h"

// Data source protocol for CreditCardEditViewController.
@protocol CreditCardEditViewControllerDataSource<
    PaymentRequestEditViewControllerDataSource>

// Returns the credit card type icon corresponding to |cardNumber|.
- (UIImage*)cardTypeIconFromCardNumber:(NSString*)cardNumber;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
