// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_SHEET_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_SHEET_CONTROLLER_MAC_
#pragma once

#import <Cocoa/Cocoa.h>
#include <vector>

@class AutoFillCreditCardModel;
@class AutoFillDialogController;
class CreditCard;

// The sheet can be invoked in "Add" or "Edit" mode.  This dictates the caption
// seen at the top of the sheet.
enum {
    kAutoFillCreditCardAddMode    =  0,
    kAutoFillCreditCardEditMode   =  1
};
typedef NSInteger AutoFillCreditCardMode;

// A class that coordinates the |creditCardModel| and the associated view
// held in AutoFillCreditCardSheet.xib.
// |initWithCreditCard:| is the designated initializer.  It takes |creditCard|
// and transcribes it to |creditCardModel| to which the view is bound.
@interface AutoFillCreditCardSheetController : NSWindowController {
 @private
  IBOutlet NSPopUpButton* billingAddressPopup_;
  IBOutlet NSPopUpButton* expirationMonthPopup_;
  IBOutlet NSPopUpButton* expirationYearPopup_;

  // The caption at top of dialog.  Text changes according to usage.  Either
  // "New credit card" or "Edit credit card" depending on context.
  IBOutlet NSTextField* caption_;

  // The credit card number field.  This is here for unit testing purposes.
  // The text of this field is obfuscated until edited.
  IBOutlet NSTextField* creditCardNumberField_;

  // The primary model for this controller.  The model is instantiated
  // from within |initWithCreditCard:|.  We do not hold it as a scoped_nsobject
  // because it is exposed as a KVO compliant property.
  AutoFillCreditCardModel* creditCardModel_;

  // Array of strings that populate the |billingAddressPopup_| control.  We
  // do not hold this as scoped_nsobject because it is exposed as a KVO
  // compliant property.  The values of this array may change as the list
  // of addresses change in the |parentController_|.
  NSArray* billingAddressContents_;

  // Array of IDs corresponding to the strings in |billingAddressContents_|.
  std::vector<int> billingAddressIDs_;

  // Contents of the expiration month and year popups.  Strongly owned.  We do
  // not hold them as scoped_nsobjects because they are exposed as KVO compliant
  // properties.
  NSArray* expirationMonthContents_;
  NSArray* expirationYearContents_;

  // A reference to our parent controller.  Used for fetching billing address
  // labels.  May be not be nil.
  // Weak reference, owns us.
  AutoFillDialogController* parentController_;

  // Either "Add" or "Edit" mode of sheet.
  AutoFillCreditCardMode mode_;
}

@property (nonatomic, retain) AutoFillCreditCardModel* creditCardModel;
@property (nonatomic, retain) NSArray* billingAddressContents;
@property (nonatomic, retain) NSArray* expirationMonthContents;
@property (nonatomic, retain) NSArray* expirationYearContents;

// Designated initializer.  Takes a copy of the data in |creditCard|,
// it is not held as a reference.
- (id)initWithCreditCard:(const CreditCard&)creditCard
                    mode:(AutoFillCreditCardMode)mode
              controller:(AutoFillDialogController*)parentController;

// IBActions for save and cancel buttons.  Both invoke |endSheet:|.
- (IBAction)save:(id)sender;
- (IBAction)cancel:(id)sender;

// Copy data from internal model to |creditCard|.
- (void)copyModelToCreditCard:(CreditCard*)creditCard;

@end

// Interface exposed for unit testing.
@interface AutoFillCreditCardSheetController (ExposedForUnitTests)
- (NSTextField*)creditCardNumberField;
@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_CREDIT_CARD_SHEET_CONTROLLER_MAC_
