// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/payment_method_selection_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
}  // namespace

@interface PaymentMethodSelectionMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSArray<PaymentMethodItem*>* items;

@end

@implementation PaymentMethodSelectionMediator

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
  return nil;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  PaymentsTextItem* addButtonItem = [[PaymentsTextItem alloc] init];
  addButtonItem.text = l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD);
  addButtonItem.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  return addButtonItem;
}

#pragma mark - Helper methods

- (NSArray<PaymentMethodItem*>*)createItems {
  const std::vector<autofill::CreditCard*>& paymentMethods =
      _paymentRequest->credit_cards();
  NSMutableArray<PaymentMethodItem*>* items =
      [NSMutableArray arrayWithCapacity:paymentMethods.size()];
  for (size_t index = 0; index < paymentMethods.size(); ++index) {
    autofill::CreditCard* paymentMethod = paymentMethods[index];
    DCHECK(paymentMethod);
    PaymentMethodItem* item = [[PaymentMethodItem alloc] init];
    item.methodID =
        base::SysUTF16ToNSString(paymentMethod->NetworkAndLastFourDigits());
    item.methodDetail = base::SysUTF16ToNSString(
        paymentMethod->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

    autofill::AutofillProfile* billingAddress =
        autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
            paymentMethod->billing_address_id(),
            _paymentRequest->billing_profiles());
    if (billingAddress) {
      item.methodAddress =
          GetBillingAddressLabelFromAutofillProfile(*billingAddress);
    }

    int methodTypeIconID =
        autofill::data_util::GetPaymentRequestData(paymentMethod->network())
            .icon_resource_id;
    item.methodTypeIcon = NativeImage(methodTypeIconID);

    item.reserveRoomForAccessoryType = YES;
    if (_paymentRequest->selected_credit_card() == paymentMethod)
      _selectedItemIndex = index;

    [items addObject:item];
  }
  return items;
}

@end
