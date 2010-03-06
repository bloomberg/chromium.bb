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
class Profile;
@class SectionSeparatorView;
@class WindowSizeAutosaver;

// A window controller for managing the autofill options dialog.
// Application modally presents a dialog allowing the user to store
// personal address and credit card information.
@interface AutoFillDialogController : NSWindowController {
 @private
  IBOutlet NSView* childView_;
  IBOutlet NSView* addressSection_;
  IBOutlet SectionSeparatorView* addressSectionBox_;
  IBOutlet NSView* creditCardSection_;

  // Note on ownership: the controllers are strongly owned by the dialog
  // controller.  Their views are inserted into the dialog's view hierarcy
  // but are retained by these controllers as well.

  // Array of |AutoFillAddressViewController|.
  scoped_nsobject<NSMutableArray> addressFormViewControllers_;

  // Array of |AutoFillCreditCardViewController|.
  scoped_nsobject<NSMutableArray> creditCardFormViewControllers_;

  AutoFillDialogObserver* observer_;  // Weak, not retained.
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> creditCards_;
  Profile* profile_;  // Weak, not retained.
  BOOL auxiliaryEnabled_;
  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;
}

// Property representing state of Address Book "me" card usage.  Checkbox is
// bound to this in nib.
@property (nonatomic) BOOL auxiliaryEnabled;

// Main interface for displaying an application modal autofill dialog on screen.
// This class method creates a new |AutoFillDialogController| and runs it as a
// modal dialog.  The controller autoreleases itself when the dialog is closed.
// |observer| can be NULL, but if it is, then no notification is sent during
// call to |save|.  If |observer| is non-NULL then its |OnAutoFillDialogApply|
// method is invoked during |save| with the new address and credit card
// information.
// |profiles| and |creditCards| must have non-NULL entries (zero or more).
// These provide the initial data that is presented to the user.
// |profile| must be non-NULL.
+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
                     creditCards:(const std::vector<CreditCard*>&)creditCards
                     profile:(Profile*)profile;

// IBActions for the dialog buttons.
- (IBAction)save:(id)sender;
- (IBAction)cancel:(id)sender;

// IBActions for adding new items.
- (IBAction)addNewAddress:(id)sender;
- (IBAction)addNewCreditCard:(id)sender;

// IBActions for deleting items.  |sender| is expected to be either a
// |AutoFillAddressViewController| or a |AutoFillCreditCardViewController|.
- (IBAction)deleteAddress:(id)sender;
- (IBAction)deleteCreditCard:(id)sender;

// IBAction for sender to alert dialog that an address label has changed.
- (IBAction)notifyAddressChange:(id)sender;

// Returns an array of strings representing the addresses in the
// |addressFormViewControllers_|.
- (NSArray*)addressStrings;

@end

// Interface exposed for unit testing.
@interface AutoFillDialogController (ExposedForUnitTests)
// Returns an instance of AutoFillDialogController.  See |-initWithObserver|
// for details about arguments.
// Note: controller is autoreleased when |-closeDialog| is called.
+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
      creditCards:(const std::vector<CreditCard*>&)creditCards
      profile:(Profile*)profile;

- (id)initWithObserver:(AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
      creditCards:(const std::vector<CreditCard*>&)creditCards
      profile:(Profile*)profile;
- (NSMutableArray*)addressFormViewControllers;
- (NSMutableArray*)creditCardFormViewControllers;
- (void)closeDialog;
@end

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
