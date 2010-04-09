// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/disclosure_view_controller.h"

@class AutoFillCreditCardModel;
@class AutoFillDialogController;
class CreditCard;

// A class that coordinates the |creditCardModel| and the associated view
// held in AutoFillCreditCardFormView.xib.
// |initWithCreditCard:| is the designated initializer.  It takes |creditCard|
// and transcribes it to |creditCardModel| to which the view is bound.
@interface AutoFillCreditCardViewController : DisclosureViewController {
 @private
  IBOutlet NSPopUpButton* billingAddressPopup_;
  IBOutlet NSPopUpButton* shippingAddressPopup_;

  // The primary model for this controller.  The model is instantiated
  // from within |initWithCreditCard:|.  We do not hold it as a scoped_nsobject
  // because it is exposed as a KVO compliant property.
  AutoFillCreditCardModel* creditCardModel_;

  // Array of strings that populate the |billingAddressPopup_| control.  We
  // do not hold this as scoped_nsobject because it is exposed as a KVO
  // compliant property.  The values of this array may change as the list
  // of addresses change in the |parentController_|.
  NSArray* billingAddressContents_;

  // Array of strings that populate the |shippingAddressPopup_| control.  We
  // do not hold this as scoped_nsobject because it is exposed as a KVO
  // compliant property.  The values of this array may change as the list
  // of addresses change in the |parentController_|.
  NSArray* shippingAddressContents_;

  // A reference to our parent controller.  Used for notifying parent if/when
  // deletion occurs.  May be not be nil.
  // Weak reference, owns us.
  AutoFillDialogController* parentController_;
}

@property (readonly) BOOL canAlterDefault;
@property BOOL isDefault;
@property (nonatomic, retain) AutoFillCreditCardModel* creditCardModel;
@property (nonatomic, retain) NSArray* billingAddressContents;
@property (nonatomic, retain) NSArray* shippingAddressContents;

// Designated initializer.  Takes a copy of the data in |creditCard|,
// it is not held as a reference.
- (id)initWithCreditCard:(const CreditCard&)creditCard
              disclosure:(NSCellStateValue)disclosureState
              controller:(AutoFillDialogController*)parentController;

// Action to remove this credit card from the dialog.  Forwards the request to
// |parentController_| which does all the actual work.  We have the action
// here so that the delete button in the AutoFillCreditCardViewFormView.xib has
// something to call.
- (IBAction)deleteCreditCard:(id)sender;

// Action to notify observers of the address list when changes have occured.
// For the credit card controller this means rebuild the popup menus.
- (IBAction)onAddressesChanged:(id)sender;

// Copy data from internal model to |creditCard|.
- (void)copyModelToCreditCard:(CreditCard*)creditCard;

@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_VIEW_CONTROLLER_MAC_
