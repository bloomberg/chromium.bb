// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "base/mac_util.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#import "chrome/browser/cocoa/disclosure_view_controller.h"
#import "chrome/browser/cocoa/section_separator_view.h"
#include "chrome/browser/profile.h"

@interface AutoFillDialogController (PrivateMethods)
- (void)runModalDialog;
- (void)installChildViews;
@end

@implementation AutoFillDialogController

+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
                     creditCards:(const std::vector<CreditCard*>&)creditCards {
  AutoFillDialogController* controller =
      [AutoFillDialogController controllerWithObserver:observer
                                      autoFillProfiles:profiles
                                           creditCards:creditCards];

  // Only run modal dialog if it is not already being shown.
  if (![controller isWindowLoaded]) {
    [controller runModalDialog];
  }
}

- (void)awakeFromNib {
  [addressSectionBox_ setShowTopLine:FALSE];
  [self installChildViews];
}

// NSWindow Delegate callback.  When the window closes the controller can
// be released.
- (void)windowWillClose:(NSNotification *)notification {
  [self autorelease];
}


// Called when the user clicks the save button.
- (IBAction)save:(id)sender {
  if (observer_) {
    [addressFormViewController_ copyModelToProfile:&profiles_[0]];
    [creditCardFormViewController_ copyModelToCreditCard:&creditCards_[0]];
    observer_->OnAutoFillDialogApply(&profiles_, &creditCards_);
  }
  [self closeDialog];
}

// Called when the user clicks the cancel button. All we need to do is stop
// the modal session.
- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

@end

@implementation AutoFillDialogController (ExposedForUnitTests)

+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
           creditCards:(const std::vector<CreditCard*>&)creditCards {

  // Deallocation is done upon window close.  See |windowWillClose:|.
  AutoFillDialogController* controller =
      [[self alloc] initWithObserver:observer
                    autoFillProfiles:profiles
                         creditCards:creditCards];
  return controller;
}


// This is the designated initializer for this class.
// |profiles| are non-retained immutable list of autofill profiles.
// |creditCards| are non-retained immutable list of credit card info.
- (id)initWithObserver:(AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
           creditCards:(const std::vector<CreditCard*>&)creditCards {
  // Use initWithWindowNibPath: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibpath = [mac_util::MainAppBundle()
                       pathForResource:@"AutoFillDialog"
                       ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    observer_ = observer;

    // Make local copy of |profiles|.
    std::vector<AutoFillProfile*>::const_iterator i;
    for (i = profiles.begin(); i != profiles.end(); ++i)
      profiles_.push_back(**i);

    // Make local copy of |creditCards|.
    std::vector<CreditCard*>::const_iterator j;
    for (j = creditCards.begin(); j != creditCards.end(); ++j)
      creditCards_.push_back(**j);
  }
  return self;
}

// Close the dialog.
- (void)closeDialog {
  [[self window] close];
  [NSApp stopModal];
}

- (AutoFillAddressViewController*)addressFormViewController {
  return addressFormViewController_.get();
}

- (AutoFillCreditCardViewController*)creditCardFormViewController {
  return creditCardFormViewController_.get();
}

@end

@implementation AutoFillDialogController (PrivateMethods)

// Run application modal.
- (void)runModalDialog {
  [NSApp runModalForWindow:[self window]];
}

// Install controller and views for the address form and the credit card form.
// They are installed into the appropriate sibling order so that they can be
// arranged vertically by the VerticalLayoutView class.  We insert the views
// into the |childView_| but we hold onto the controllers and release them in
// our dealloc once the dialog closes.
- (void)installChildViews {
  if (profiles_.size() > 0) {
    AutoFillAddressViewController* autoFillAddressViewController =
        [[AutoFillAddressViewController alloc] initWithProfile:profiles_[0]];
    addressFormViewController_.reset(autoFillAddressViewController);

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[addressFormViewController_ view]
                positioned:NSWindowBelow relativeTo:addressSection_];
    [[addressFormViewController_ view] setFrameOrigin:NSMakePoint(0, 0)];
  }

  if (creditCards_.size() > 0) {
    AutoFillCreditCardViewController* autoFillCreditCardViewController =
        [[AutoFillCreditCardViewController alloc]
            initWithCreditCard:creditCards_[0]];
    creditCardFormViewController_.reset(autoFillCreditCardViewController);

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[creditCardFormViewController_ view]
                positioned:NSWindowBelow relativeTo:creditCardSection_];
    [[creditCardFormViewController_ view] setFrameOrigin:NSMakePoint(0, 0)];
  }
}

@end
