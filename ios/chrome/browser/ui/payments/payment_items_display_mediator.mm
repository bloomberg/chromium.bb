// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/ui/payments/payment_items_display_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentItemsDisplayMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

@end

@implementation PaymentItemsDisplayMediator

@synthesize paymentRequest = _paymentRequest;

- (instancetype)initWithPaymentRequest:
    (payments::PaymentRequest*)paymentRequest {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
  }
  return self;
}

#pragma mark - PaymentItemsDisplayViewControllerDataSource

- (CollectionViewItem*)totalItem {
  PriceItem* totalItem = [[PriceItem alloc] init];
  totalItem.item =
      base::SysUTF16ToNSString(_paymentRequest->payment_details().total.label);
  payments::CurrencyFormatter* currencyFormatter =
      _paymentRequest->GetOrCreateCurrencyFormatter();
  totalItem.price = SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(currencyFormatter->formatted_currency_code()),
      currencyFormatter->Format(base::UTF16ToASCII(
          _paymentRequest->payment_details().total.amount.value))));
  return totalItem;
}

- (NSArray<CollectionViewItem*>*)lineItems {
  const std::vector<web::PaymentItem>& paymentItems =
      _paymentRequest->payment_details().display_items;
  NSMutableArray<CollectionViewItem*>* lineItems =
      [NSMutableArray arrayWithCapacity:paymentItems.size()];

  for (const auto& paymentItem : paymentItems) {
    PriceItem* item = [[PriceItem alloc] init];
    item.item = base::SysUTF16ToNSString(paymentItem.label);
    payments::CurrencyFormatter* currencyFormatter =
        _paymentRequest->GetOrCreateCurrencyFormatter();
    item.price = SysUTF16ToNSString(currencyFormatter->Format(
        base::UTF16ToASCII(paymentItem.amount.value)));

    [lineItems addObject:item];
  }
  return lineItems;
}

@end
