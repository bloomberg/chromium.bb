// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/disclosure_view_controller.h"

@class AutoFillCreditCardModel;
class CreditCard;

// A class that coordinates the |creditCardModel| and the associated view
// held in AutoFillCreditCardFormView.xib.
// |initWithCreditCard:| is the designated initializer.  It takes |creditCard|
// and transcribes it to |creditCardModel| to which the view is bound.
@interface AutoFillCreditCardViewController : DisclosureViewController {
 @private
  // TODO(dhollowa): temporary to disable until implementend.
  // See http://crbug.com/33029.
  IBOutlet NSTextField* billingAddressLabel_;
  IBOutlet NSPopUpButton* billingAddressPopup_;
  IBOutlet NSTextField* shippingAddressLabel_;
  IBOutlet NSPopUpButton* shippingAddressPopup_;

  // The primary model for this controller.  The model is instantiated
  // from within |initWithCreditCard:|.  We do not hold it as a scoped_nsobject
  // because it is exposed as a KVO compliant property.
  AutoFillCreditCardModel* creditCardModel_;
}

@property (nonatomic, retain) AutoFillCreditCardModel* creditCardModel;

// Designated initializer.  Takes a copy of the data in |creditCard|,
// it is not held as a reference.
- (id)initWithCreditCard:(const CreditCard&)creditCard;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_
