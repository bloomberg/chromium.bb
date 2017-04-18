// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/payments/shipping_address_selection_mediator.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
}  // namespace

@interface ShippingAddressSelectionMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSArray<AutofillProfileItem*>* items;

@end

@implementation ShippingAddressSelectionMediator

@synthesize state = _state;
@synthesize selectedItemIndex = _selectedItemIndex;
@synthesize paymentRequest = _paymentRequest;
@synthesize items = _items;
@synthesize headerText = _headerText;

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

- (CollectionViewItem*)selectableItemAtIndex:(NSUInteger)index {
  DCHECK(index < self.items.count);
  return [self.items objectAtIndex:index];
}

- (CollectionViewItem*)addButtonItem {
  PaymentsTextItem* addButtonItem = [[PaymentsTextItem alloc] init];
  addButtonItem.text = l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS);
  addButtonItem.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  return addButtonItem;
}

#pragma mark - Helper methods

- (NSArray<AutofillProfileItem*>*)createItems {
  const std::vector<autofill::AutofillProfile*>& shippingProfiles =
      _paymentRequest->shipping_profiles();
  NSMutableArray<AutofillProfileItem*>* items =
      [NSMutableArray arrayWithCapacity:shippingProfiles.size()];
  for (size_t index = 0; index < shippingProfiles.size(); ++index) {
    autofill::AutofillProfile* shippingAddress = shippingProfiles[index];
    DCHECK(shippingAddress);
    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    item.name = GetNameLabelFromAutofillProfile(*shippingAddress);
    item.address = GetShippingAddressLabelFromAutofillProfile(*shippingAddress);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*shippingAddress);
    if (_paymentRequest->selected_shipping_profile() == shippingAddress)
      _selectedItemIndex = index;

    [items addObject:item];
  }
  return items;
}

@end
