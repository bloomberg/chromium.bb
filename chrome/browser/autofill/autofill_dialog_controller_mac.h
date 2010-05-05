// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_

#import <Cocoa/Cocoa.h>
#include <vector>
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"

namespace AutoFillDialogControllerInternal {
class PersonalDataManagerObserver;
}  // AutoFillDialogControllerInternal

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
  // controller.  Their views are inserted into the dialog's view hierarchy
  // but are retained by these controllers as well.

  // Array of |AutoFillAddressViewController|.
  scoped_nsobject<NSMutableArray> addressFormViewControllers_;

  // Array of |AutoFillCreditCardViewController|.
  scoped_nsobject<NSMutableArray> creditCardFormViewControllers_;

  scoped_nsobject<NSString> defaultAddressLabel_;
  scoped_nsobject<NSString> defaultCreditCardLabel_;

  AutoFillDialogObserver* observer_;  // Weak, not retained.
  Profile* profile_;  // Weak, not retained.
  AutoFillProfile* importedProfile_;  // Weak, not retained.
  CreditCard* importedCreditCard_;  // Weak, not retained.
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> creditCards_;
  BOOL auxiliaryEnabled_;
  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;

  // Manages PersonalDataManager loading.
  scoped_ptr<AutoFillDialogControllerInternal::PersonalDataManagerObserver>
      personalDataManagerObserver_;
}

// Property representing state of Address Book "me" card usage.  Checkbox is
// bound to this in nib.
@property (nonatomic) BOOL auxiliaryEnabled;

// Property representing the default profile and credit card.
@property (nonatomic, copy) NSString* defaultAddressLabel;
@property (nonatomic, copy) NSString* defaultCreditCardLabel;

// Main interface for displaying an application modal autofill dialog on screen.
// This class method creates a new |AutoFillDialogController| and runs it as a
// modal dialog.  The controller autoreleases itself when the dialog is closed.
// |observer| can be NULL, but if it is, then no notification is sent during
// call to |save|.  If |observer| is non-NULL then its |OnAutoFillDialogApply|
// method is invoked during |save| with the new address and credit card
// information.
// |profile| must be non-NULL.
// AutoFill profile and credit card data is initialized from the
// |PersonalDataManager| that is associated with the input |profile|.
// If |importedProfile| or |importedCreditCard| parameters are supplied then
// the |PersonalDataManager| data is ignored.  Both may be NULL.
+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                     profile:(Profile*)profile
             importedProfile:(AutoFillProfile*)importedProfile
          importedCreditCard:(CreditCard*)importedCreditCard;

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

// Returns an array of labels representing the addresses in the
// |addressFormViewControllers_|.
- (NSArray*)addressLabels;

// Returns an array of labels representing the credit cards in the
// |creditCardFormViewControllers_|.
- (NSArray*)creditCardLabels;

@end

// Interface exposed for unit testing.
@interface AutoFillDialogController (ExposedForUnitTests)
// Returns an instance of AutoFillDialogController.  See |-initWithObserver|
// for details about arguments.
// Note: controller is autoreleased when |-closeDialog| is called.
+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
               profile:(Profile*)profile
       importedProfile:(AutoFillProfile*)importedProfile
    importedCreditCard:(CreditCard*)importedCreditCard;

- (id)initWithObserver:(AutoFillDialogObserver*)observer
               profile:(Profile*)profile
       importedProfile:(AutoFillProfile*)importedProfile
    importedCreditCard:(CreditCard*)importedCreditCard;
- (NSMutableArray*)addressFormViewControllers;
- (NSMutableArray*)creditCardFormViewControllers;
- (void)closeDialog;
@end

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
