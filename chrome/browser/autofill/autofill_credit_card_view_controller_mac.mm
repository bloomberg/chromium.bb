// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#include "chrome/browser/autofill/credit_card.h"

@implementation AutoFillCreditCardViewController

@synthesize creditCardModel = creditCardModel_;

- (id)initWithCreditCard:(const CreditCard&)creditCard {
  self = [super initWithNibName:@"AutoFillCreditCardFormView"
                         bundle:mac_util::MainAppBundle()];
  if (self) {
    // Pull in the view for initialization.
    [self view];

    // Create the model.
    [self setCreditCardModel:[[[AutoFillCreditCardModel alloc]
        initWithCreditCard:creditCard] autorelease]];

    // Setup initial state.
    // TODO(dhollowa): not yet implemented, disabling controls for now.
    // See http://crbug.com/33029.
    [billingAddressLabel_ setEnabled:FALSE];
    [billingAddressLabel_ setTextColor:[NSColor secondarySelectedControlColor]];
    [billingAddressPopup_ removeAllItems];
    [billingAddressPopup_ setEnabled:FALSE];
    [shippingAddressLabel_ setEnabled:FALSE];
    [shippingAddressLabel_ setTextColor:
        [NSColor secondarySelectedControlColor]];
    [shippingAddressPopup_ removeAllItems];
    [shippingAddressPopup_ setEnabled:FALSE];
  }
  return self;
}

- (void)dealloc {
  [creditCardModel_ release];
  [super dealloc];
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  [creditCardModel_ copyModelToCreditCard:creditCard];
}

@end
