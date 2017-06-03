// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/shipping_option_selection_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/currency_formatter.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShippingOptionSelectionMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSArray<PaymentsTextItem*>* items;

@end

@implementation ShippingOptionSelectionMediator

@synthesize headerText = _headerText;
@synthesize state = _state;
@synthesize selectedItemIndex = _selectedItemIndex;
@synthesize paymentRequest = _paymentRequest;
@synthesize items = _items;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _selectedItemIndex = NSUIntegerMax;
    _items = [self createItems];
  }
  return self;
}

#pragma mark - PaymentRequestSelectorViewControllerDataSource

- (CollectionViewItem*)headerItem {
  if (!self.headerText.length)
    return nil;

  PaymentsTextItem* headerItem = [[PaymentsTextItem alloc] init];
  headerItem.text = self.headerText;
  if (self.state == PaymentRequestSelectorStateError)
    headerItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
  return headerItem;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  return nil;
}

#pragma mark - Helper methods

- (NSArray<PaymentsTextItem*>*)createItems {
  const std::vector<web::PaymentShippingOption*>& shippingOptions =
      _paymentRequest->shipping_options();
  NSMutableArray<PaymentsTextItem*>* items =
      [NSMutableArray arrayWithCapacity:shippingOptions.size()];
  for (size_t index = 0; index < shippingOptions.size(); ++index) {
    web::PaymentShippingOption* shippingOption = shippingOptions[index];
    DCHECK(shippingOption);
    PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
    item.text = base::SysUTF16ToNSString(shippingOption->label);
    payments::CurrencyFormatter* currencyFormatter =
        _paymentRequest->GetOrCreateCurrencyFormatter();
    item.detailText = SysUTF16ToNSString(currencyFormatter->Format(
        base::UTF16ToASCII(shippingOption->amount.value)));
    if (_paymentRequest->selected_shipping_option() == shippingOption)
      _selectedItemIndex = index;

    [items addObject:item];
  }
  return items;
}

@end
