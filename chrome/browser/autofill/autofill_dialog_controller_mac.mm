// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "app/l10n_util.h"
#include "base/mac_util.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/disclosure_view_controller.h"
#import "chrome/browser/cocoa/section_separator_view.h"
#import "chrome/browser/cocoa/window_size_autosaver.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

@interface AutoFillDialogController (PrivateMethods)
- (void)runModalDialog;
- (void)installChildViews;
@end

@implementation AutoFillDialogController

@synthesize auxiliaryEnabled = auxiliaryEnabled_;

+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
                     creditCards:(const std::vector<CreditCard*>&)creditCards
                         profile:(Profile*)profile {
  AutoFillDialogController* controller =
      [AutoFillDialogController controllerWithObserver:observer
          autoFillProfiles:profiles
               creditCards:creditCards
                   profile:profile];

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
  // Call |makeFirstResponder:| to commit pending text field edits.
  [[self window] makeFirstResponder:[self window]];

  // If we have an |observer_| then communicate the changes back.
  if (observer_) {
    profiles_.clear();
    profiles_.resize([addressFormViewControllers_ count]);
    int i = 0;
    for (AutoFillAddressViewController* addressFormViewController in
        addressFormViewControllers_.get()) {
      // Initialize the profile here.  The default initializer does not fully
      // initialize.
      profiles_[i] = AutoFillProfile(ASCIIToUTF16(""), 0);
      [addressFormViewController copyModelToProfile:&profiles_[i]];
      i++;
    }
    creditCards_.clear();
    creditCards_.resize([creditCardFormViewControllers_ count]);
    int j = 0;
    for (AutoFillCreditCardViewController* creditCardFormViewController in
        creditCardFormViewControllers_.get()) {
      // Initialize the credit card here.  The default initializer does not
      // fully initialize.
      creditCards_[j] = CreditCard(ASCIIToUTF16(""), 0);
      [creditCardFormViewController copyModelToCreditCard:&creditCards_[j]];
      j++;
    }
    profile_->GetPrefs()->SetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled,
                                     auxiliaryEnabled_);
    observer_->OnAutoFillDialogApply(&profiles_, &creditCards_);
  }
  [self closeDialog];
}

// Called when the user clicks the cancel button. All we need to do is stop
// the modal session.
- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

// Adds new address to bottom of list.  A new address controller is created
// and its view is inserted into the view heirarchy.
- (IBAction)addNewAddress:(id)sender {
  // Insert relative to top of section, or below last address.
  NSView* insertionPoint;
  NSUInteger count = [addressFormViewControllers_.get() count];
  if (count == 0) {
    insertionPoint = addressSection_;
  } else {
    insertionPoint = [[addressFormViewControllers_.get()
        objectAtIndex:[addressFormViewControllers_.get() count] - 1] view];
  }

  // Create a new default address, and add it to our array of controllers.
  string16 new_address_name = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_NEW_ADDRESS);
  AutoFillProfile newProfile(new_address_name, 0);
  AutoFillAddressViewController* addressViewController =
      [[AutoFillAddressViewController alloc]
          initWithProfile:newProfile
               disclosure:NSOnState
               controller:self];
  [addressFormViewControllers_.get() addObject:addressViewController];

  // Embed the new address into our target view.
  [childView_ addSubview:[addressViewController view]
      positioned:NSWindowBelow relativeTo:insertionPoint];
  [[addressViewController view] setFrameOrigin:NSMakePoint(0, 0)];

  [self notifyAddressChange:self];

  // Recalculate key view loop to account for change in view tree.
  [[self window] recalculateKeyViewLoop];
}

// Adds new credit card to bottom of list.  A new credit card controller is
// created and its view is inserted into the view heirarchy.
- (IBAction)addNewCreditCard:(id)sender {
  // Insert relative to top of section, or below last address.
  NSView* insertionPoint;
  NSUInteger count = [creditCardFormViewControllers_.get() count];
  if (count == 0) {
    insertionPoint = creditCardSection_;
  } else {
    insertionPoint = [[creditCardFormViewControllers_.get()
        objectAtIndex:[creditCardFormViewControllers_.get() count] - 1] view];
  }

  // Create a new default credit card, and add it to our array of controllers.
  string16 new_credit_card_name = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_NEW_CREDITCARD);
  CreditCard newCreditCard(new_credit_card_name, 0);
  AutoFillCreditCardViewController* creditCardViewController =
      [[AutoFillCreditCardViewController alloc]
          initWithCreditCard:newCreditCard
                  disclosure:NSOnState
                  controller:self];
  [creditCardFormViewControllers_.get() addObject:creditCardViewController];

  // Embed the new address into our target view.
  [childView_ addSubview:[creditCardViewController view]
      positioned:NSWindowBelow relativeTo:insertionPoint];
  [[creditCardViewController view] setFrameOrigin:NSMakePoint(0, 0)];

  // Recalculate key view loop to account for change in view tree.
  [[self window] recalculateKeyViewLoop];
}

- (IBAction)deleteAddress:(id)sender {
  NSUInteger i = [addressFormViewControllers_.get() indexOfObject:sender];
  DCHECK(i != NSNotFound);

  // Remove controller's view from superview and remove from list of
  // controllers.  Note on lifetime: removing view from super view decrements
  // refcount of view, removing controller from array decrements refcount of
  // controller which in-turn decrement refcount of view.  Both should dealloc
  // at this point.
  [[sender view] removeFromSuperview];
  [addressFormViewControllers_.get() removeObjectAtIndex:i];

  [self notifyAddressChange:self];

  // Recalculate key view loop to account for change in view tree.
  [[self window] recalculateKeyViewLoop];
}

- (IBAction)deleteCreditCard:(id)sender {
  NSUInteger i = [creditCardFormViewControllers_.get() indexOfObject:sender];
  DCHECK(i != NSNotFound);

  // Remove controller's view from superview and remove from list of
  // controllers.  Note on lifetime: removing view from super view decrements
  // refcount of view, removing controller from array decrements refcount of
  // controller which in-turn decrement refcount of view.  Both should dealloc
  // at this point.
  [[sender view] removeFromSuperview];
  [creditCardFormViewControllers_.get() removeObjectAtIndex:i];

  // Recalculate key view loop to account for change in view tree.
  [[self window] recalculateKeyViewLoop];
}

// Credit card controllers are dependent upon the address labels.  So we notify
// them here that something has changed.
- (IBAction)notifyAddressChange:(id)sender {
  for (AutoFillCreditCardViewController* creditCardFormViewController in
      creditCardFormViewControllers_.get()) {
      [creditCardFormViewController onAddressesChanged:self];
  }
}

// Returns an array of strings representing the addresses in the
// |addressFormViewControllers_|.
- (NSArray*)addressStrings {
  NSUInteger capacity = [addressFormViewControllers_ count];
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:capacity];

  for (AutoFillAddressViewController* addressFormViewController in
      addressFormViewControllers_.get()) {
    [array addObject:[[addressFormViewController addressModel] label]];
  }

  return array;
}

@end

@implementation AutoFillDialogController (ExposedForUnitTests)

+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
      creditCards:(const std::vector<CreditCard*>&)creditCards
      profile:(Profile*)profile {

  // Deallocation is done upon window close.  See |windowWillClose:|.
  AutoFillDialogController* controller =
      [[self alloc] initWithObserver:observer
                    autoFillProfiles:profiles
                         creditCards:creditCards
                             profile:profile];
  return controller;
}


// This is the designated initializer for this class.
// |profiles| are non-retained immutable list of autofill profiles.
// |creditCards| are non-retained immutable list of credit card info.
- (id)initWithObserver:(AutoFillDialogObserver*)observer
      autoFillProfiles:(const std::vector<AutoFillProfile*>&)profiles
           creditCards:(const std::vector<CreditCard*>&)creditCards
               profile:(Profile*)profile {
  CHECK(profile);
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

    profile_ = profile;

    // Use property here to trigger KVO binding.
    [self setAuxiliaryEnabled:profile_->GetPrefs()->GetBoolean(
        prefs::kAutoFillAuxiliaryProfilesEnabled)];

    // Initialize array of sub-controllers.
    addressFormViewControllers_.reset([[NSMutableArray array] retain]);

    // Initialize array of sub-controllers.
    creditCardFormViewControllers_.reset([[NSMutableArray array] retain]);
  }
  return self;
}

// Close the dialog.
- (void)closeDialog {
  [[self window] close];
  [NSApp stopModal];
}

- (NSMutableArray*)addressFormViewControllers {
  return addressFormViewControllers_.get();
}

- (NSMutableArray*)creditCardFormViewControllers {
  return creditCardFormViewControllers_.get();
}

@end

@implementation AutoFillDialogController (PrivateMethods)

// Run application modal.
- (void)runModalDialog {
  // Use stored window geometry if it exists.
  if (g_browser_process && g_browser_process->local_state()) {
    sizeSaver_.reset([[WindowSizeAutosaver alloc]
        initWithWindow:[self window]
           prefService:g_browser_process->local_state()
                  path:prefs::kAutoFillDialogPlacement
                 state:kSaveWindowPos]);
  }

  [NSApp runModalForWindow:[self window]];
}

// Install controller and views for the address form and the credit card form.
// They are installed into the appropriate sibling order so that they can be
// arranged vertically by the VerticalLayoutView class.  We insert the views
// into the |childView_| but we hold onto the controllers and release them in
// our dealloc once the dialog closes.
- (void)installChildViews {
  NSView* insertionPoint;
  insertionPoint = addressSection_;
  for (size_t i = 0; i < profiles_.size(); i++) {
    // Special case for first address, we want to show full contents.
    NSCellStateValue disclosureState = (i == 0) ? NSOnState : NSOffState;
    AutoFillAddressViewController* addressViewController =
        [[AutoFillAddressViewController alloc]
            initWithProfile:profiles_[i]
                 disclosure:disclosureState
                 controller:self];
    [addressFormViewControllers_.get() addObject:addressViewController];

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[addressViewController view]
                positioned:NSWindowBelow relativeTo:insertionPoint];
    insertionPoint = [addressViewController view];
    [[addressViewController view] setFrameOrigin:NSMakePoint(0, 0)];
  }

  insertionPoint = creditCardSection_;
  for (size_t i = 0; i < creditCards_.size(); i++) {
    AutoFillCreditCardViewController* creditCardViewController =
        [[AutoFillCreditCardViewController alloc]
            initWithCreditCard:creditCards_[i]
                    disclosure:NSOffState
                    controller:self];
    [creditCardFormViewControllers_.get() addObject:creditCardViewController];

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[creditCardViewController view]
                positioned:NSWindowBelow relativeTo:insertionPoint];
    insertionPoint = [creditCardViewController view];
    [[creditCardViewController view] setFrameOrigin:NSMakePoint(0, 0)];
  }
}

@end
