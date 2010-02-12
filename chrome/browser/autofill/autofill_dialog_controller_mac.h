// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_

#import <Cocoa/Cocoa.h>
#include <vector>
#include "base/scoped_nsobject.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"

@class AutoFillAddressViewController;
@class AutoFillCreditCardViewController;
@class SectionSeparatorView;

// A window controller for managing the autofill options dialog.
// Application modally presents a dialog allowing the user to store
// personal address and credit card information.
@interface AutoFillDialogController : NSWindowController {
 @private
  IBOutlet NSView* childView_;
  IBOutlet NSView* addressSection_;
  IBOutlet SectionSeparatorView* addressSectionBox_;
  IBOutlet NSView* creditCardSection_;

  // TODO(dhollowa): one each of these for now.  Will be n of each
  // controller eventually, for n addresses and n credit cards.
  // Note on ownership: the controllers are strongly owned by the dialog
  // controller.  Their views are inserted into the dialog's view hierarcy
  // but are retained by these controllers as well.
  // See http://crbug.com/33029.
  scoped_nsobject<AutoFillAddressViewController>
      addressFormViewController_;
  scoped_nsobject<AutoFillCreditCardViewController>
      creditCardFormViewController_;

  AutoFillDialogObserver* observer_;  // (weak) not retained
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> creditCards_;
}

// Main interface for displaying an application modal autofill dialog on screen.
// This class method creates a new |AutoFillDialogController| and runs it as a
// modal dialog.  The controller autoreleases itself when the dialog is closed.
// |observer| can be NULL, but if it is, then no notification is sent during
// call to |save|.  If |observer| is non-NULL then its |OnAutoFillDialogApply|
// method is invoked during |save| with the new address and credit card
// information.
// |profiles| and |creditCards| must have non-NULL entries (zero or more).
// These provide the initial data that is presented to the user.
+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
                     creditCards:(const std::vector<CreditCard*>&)creditCards;

// IBActions for the dialog buttons.
- (IBAction)save:(id)sender;
- (IBAction)cancel:(id)sender;

@end

// Interface exposed for unit testing.
@interface AutoFillDialogController (ExposedForUnitTests)
// Returns an instance of AutoFillDialogController.  See |-initWithObserver|
// for details about arguments.
// Note: controller is autoreleased when |-closeDialog| is called.
+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
           creditCards:(const std::vector<CreditCard*>&)creditCards;

- (id)initWithObserver:(AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
           creditCards:(const std::vector<CreditCard*>&)creditCards;
- (AutoFillAddressViewController*)addressFormViewController;
- (AutoFillCreditCardViewController*)creditCardFormViewController;
- (void)closeDialog;
@end

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
