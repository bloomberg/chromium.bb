// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#import "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/disclosure_view_controller.h"
#import "chrome/browser/cocoa/section_separator_view.h"
#import "chrome/browser/cocoa/window_size_autosaver.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

// Private interface.
@interface AutoFillDialogController (PrivateAPI)
// Asyncronous handler for when PersonalDataManager data loads.  The
// personal data manager notifies the dialog with this method when the
// data loading is complete and ready to be used.
- (void)onPersonalDataLoaded:(const std::vector<AutoFillProfile*>&)profiles
                 creditCards:(const std::vector<CreditCard*>&)creditCards;
@end

namespace AutoFillDialogControllerInternal {

// PersonalDataManagerObserver facilitates asynchronous loading of
// PersonalDataManager data before showing the AutoFill settings data to the
// user.  It acts as a C++-based delegate for the |AutoFillDialogController|.
class PersonalDataManagerObserver : public PersonalDataManager::Observer {
 public:
  explicit PersonalDataManagerObserver(
      AutoFillDialogController* controller,
      PersonalDataManager* personal_data_manager,
      Profile* profile)
      : controller_(controller),
        personal_data_manager_(personal_data_manager),
        profile_(profile) {
  }

  virtual ~PersonalDataManagerObserver();

  // Notifies the observer that the PersonalDataManager has finished loading.
  virtual void OnPersonalDataLoaded();

 private:
  // Utility method to remove |this| from |personal_data_manager_| as an
  // observer.
  void RemoveObserver();

  // The dialog controller to be notified when the data loading completes.
  // Weak reference.
  AutoFillDialogController* controller_;

  // The object in which we are registered as an observer.  We hold on to
  // it to facilitate un-registering ourself in the destructor and in the
  // |OnPersonalDataLoaded| method.  This may be NULL.
  // Weak reference.
  PersonalDataManager* personal_data_manager_;

  // Profile of caller.  Held as weak reference.  May not be NULL.
  Profile* profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerObserver);
};

// During destruction ensure that we are removed from the
// |personal_data_manager_| as an observer.
PersonalDataManagerObserver::~PersonalDataManagerObserver() {
  RemoveObserver();
}

void PersonalDataManagerObserver::RemoveObserver() {
  if (personal_data_manager_) {
    personal_data_manager_->RemoveObserver(this);
  }
}

// The data is ready so display our data.  Notify the dialog controller that
// the data is ready.  Once done we clear the observer.
void PersonalDataManagerObserver::OnPersonalDataLoaded() {
  RemoveObserver();
  [controller_ onPersonalDataLoaded:personal_data_manager_->web_profiles()
                        creditCards:personal_data_manager_->credit_cards()];
}

}  // namespace AutoFillDialogControllerInternal

@interface AutoFillDialogController (PrivateMethods)
- (void)runModalDialog;
- (void)installChildViews;
@end

@implementation AutoFillDialogController

@synthesize auxiliaryEnabled = auxiliaryEnabled_;

+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
               profile:(Profile*)profile
       importedProfile:(AutoFillProfile*) importedProfile
    importedCreditCard:(CreditCard*) importedCreditCard {
  AutoFillDialogController* controller =
      [AutoFillDialogController controllerWithObserver:observer
                                         profile:profile
                                 importedProfile:importedProfile
                              importedCreditCard:importedCreditCard];

  // Only run modal dialog if it is not already being shown.
  if (![controller isWindowLoaded]) {
    [controller runModalDialog];
  }
}

- (void)awakeFromNib {
  [addressSectionBox_ setShowTopLine:FALSE];

  PersonalDataManager* personal_data_manager =
      profile_->GetPersonalDataManager();
  DCHECK(personal_data_manager);

  if (personal_data_manager->IsDataLoaded()) {
    // |personalDataManager| data is loaded, we can proceed with the contents.
    [self onPersonalDataLoaded:personal_data_manager->web_profiles()
                   creditCards:personal_data_manager->credit_cards()];
  } else {
    // |personalDataManager| data is NOT loaded, so we load it here, installing
    // our observer.
    personalDataManagerObserver_.reset(
        new AutoFillDialogControllerInternal::PersonalDataManagerObserver(
            self, personal_data_manager, profile_));
    personal_data_manager->SetObserver(personalDataManagerObserver_.get());
  }
}

// NSWindow Delegate callback.  When the window closes the controller can
// be released.
- (void)windowWillClose:(NSNotification *)notification {
  // Force views to go away so they properly remove their observations.
  addressFormViewControllers_.reset();
  creditCardFormViewControllers_.reset();
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
    // Make sure to use accessors here to save what the user sees.
    profile_->GetPrefs()->SetString(
        prefs::kAutoFillDefaultProfile,
        base::SysNSStringToWide([self defaultAddressLabel]));
    profile_->GetPrefs()->SetString(
        prefs::kAutoFillDefaultCreditCard,
        base::SysNSStringToWide([self defaultCreditCardLabel]));
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
// and its view is inserted into the view hierarchy.
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
  scoped_nsobject<AutoFillAddressViewController> addressViewController(
      [[AutoFillAddressViewController alloc]
          initWithProfile:newProfile
               disclosure:NSOnState
               controller:self]);
  [self willChangeValueForKey:@"addressLabels"];
  [addressFormViewControllers_.get() addObject:addressViewController];
  [self didChangeValueForKey:@"addressLabels"];
  // Might need to reset the default if added.
  [self willChangeValueForKey:@"defaultAddressLabel"];
  [self didChangeValueForKey:@"defaultAddressLabel"];

  // Embed the new address into our target view.
  [childView_ addSubview:[addressViewController view]
      positioned:NSWindowBelow relativeTo:insertionPoint];
  [[addressViewController view] setFrameOrigin:NSMakePoint(0, 0)];

  [self notifyAddressChange:self];

  // Recalculate key view loop to account for change in view tree.
  [[self window] recalculateKeyViewLoop];
}

// Adds new credit card to bottom of list.  A new credit card controller is
// created and its view is inserted into the view hierarchy.
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
  scoped_nsobject<AutoFillCreditCardViewController> creditCardViewController(
      [[AutoFillCreditCardViewController alloc]
          initWithCreditCard:newCreditCard
                  disclosure:NSOnState
                  controller:self]);
  [self willChangeValueForKey:@"creditCardLabels"];
  [creditCardFormViewControllers_.get() addObject:creditCardViewController];
  [self didChangeValueForKey:@"creditCardLabels"];
  // Might need to reset the default if added.
  [self willChangeValueForKey:@"defaultCreditCardLabel"];
  [self didChangeValueForKey:@"defaultCreditCardLabel"];

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
  [self willChangeValueForKey:@"addressLabels"];
  [addressFormViewControllers_.get() removeObjectAtIndex:i];
  [self didChangeValueForKey:@"addressLabels"];
  // Might need to reset the default if deleted.
  [self willChangeValueForKey:@"defaultAddressLabel"];
  [self didChangeValueForKey:@"defaultAddressLabel"];

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
  [self willChangeValueForKey:@"creditCardLabels"];
  [creditCardFormViewControllers_.get() removeObjectAtIndex:i];
  [self didChangeValueForKey:@"creditCardLabels"];
  // Might need to reset the default if deleted.
  [self willChangeValueForKey:@"defaultCreditCardLabel"];
  [self didChangeValueForKey:@"defaultCreditCardLabel"];

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

- (NSArray*)addressLabels {
  NSUInteger capacity = [addressFormViewControllers_ count];
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:capacity];

  for (AutoFillAddressViewController* addressFormViewController in
      addressFormViewControllers_.get()) {
    [array addObject:[[addressFormViewController addressModel] label]];
  }

  return array;
}

- (NSArray*)creditCardLabels {
  NSUInteger capacity = [creditCardFormViewControllers_ count];
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:capacity];

  for (AutoFillCreditCardViewController* creditCardFormViewController in
      creditCardFormViewControllers_.get()) {
    [array addObject:[[creditCardFormViewController creditCardModel] label]];
  }

  return array;
}

- (NSString*)defaultAddressLabel {
  NSArray* labels = [self addressLabels];
  NSString* def = defaultAddressLabel_.get();
  if ([def length] && [labels containsObject:def])
    return def;

  // No valid default; pick the first item.
  if ([labels count]) {
    return [labels objectAtIndex:0];
  } else {
    return @"";
  }
}

- (void)setDefaultAddressLabel:(NSString*)label {
  if (!label) {
    // Setting nil means the user un-checked an item. Find a new default.
    NSUInteger itemCount = [addressFormViewControllers_ count];
    if (itemCount == 0) {
      DCHECK(false) << "Attempt to set default when there are no items.";
      return;
    } else if (itemCount == 1) {
      DCHECK(false) << "Attempt to set default when there is only one item, so "
                       "it should have been disabled.";
      AutoFillAddressViewController* controller =
          [addressFormViewControllers_ objectAtIndex:0];
      label = [[controller addressModel] label];
    } else {
      AutoFillAddressViewController* controller =
          [addressFormViewControllers_ objectAtIndex:0];
      NSString* firstItemsLabel = [[controller addressModel] label];

      // If they unchecked an item that wasn't the first item, make the first
      // item default.
      if (![defaultAddressLabel_ isEqual:firstItemsLabel]) {
        label = firstItemsLabel;
      } else {
        // Otherwise they unchecked the first item. Pick the second one for 'em.
        AutoFillAddressViewController* controller =
            [addressFormViewControllers_ objectAtIndex:1];
        label = [[controller addressModel] label];
      }
    }
  }

  defaultAddressLabel_.reset([label copy]);
  return;
}

- (NSString*)defaultCreditCardLabel {
  NSArray* labels = [self creditCardLabels];
  NSString* def = defaultCreditCardLabel_.get();
  if ([def length] && [labels containsObject:def])
    return def;

  // No valid default; pick the first item.
  if ([labels count]) {
    return [labels objectAtIndex:0];
  } else {
    return @"";
  }
}

- (void)setDefaultCreditCardLabel:(NSString*)label {
  if (!label) {
    // Setting nil means the user un-checked an item. Find a new default.
    NSUInteger itemCount = [creditCardFormViewControllers_ count];
    if (itemCount == 0) {
      DCHECK(false) << "Attempt to set default when there are no items.";
      return;
    } else if (itemCount == 1) {
      DCHECK(false) << "Attempt to set default when there is only one item, so "
                       "it should have been disabled.";
      AutoFillCreditCardViewController* controller =
         [creditCardFormViewControllers_ objectAtIndex:0];
      label = [[controller creditCardModel] label];
    } else {
      AutoFillCreditCardViewController* controller =
         [creditCardFormViewControllers_ objectAtIndex:0];
      NSString* firstItemsLabel = [[controller creditCardModel] label];

      // If they unchecked an item that wasn't the first item, make the first
      // item default.
      if (![defaultCreditCardLabel_ isEqual:firstItemsLabel]) {
        label = firstItemsLabel;
      } else {
        // Otherwise they unchecked the first item. Pick the second one for 'em.
        AutoFillCreditCardViewController* controller =
           [creditCardFormViewControllers_ objectAtIndex:1];
        label = [[controller creditCardModel] label];
      }
    }
  }

  defaultCreditCardLabel_.reset([label copy]);
  return;
}

@end

@implementation AutoFillDialogController (ExposedForUnitTests)

+ (AutoFillDialogController*)controllerWithObserver:
        (AutoFillDialogObserver*)observer
               profile:(Profile*)profile
       importedProfile:(AutoFillProfile*)importedProfile
    importedCreditCard:(CreditCard*)importedCreditCard {

  // Deallocation is done upon window close.  See |windowWillClose:|.
  AutoFillDialogController* controller =
      [[self alloc] initWithObserver:observer
                             profile:profile
                     importedProfile:importedProfile
                  importedCreditCard:importedCreditCard];
  return controller;
}


// This is the designated initializer for this class.
// |profiles| are non-retained immutable list of autofill profiles.
// |creditCards| are non-retained immutable list of credit card info.
- (id)initWithObserver:(AutoFillDialogObserver*)observer
               profile:(Profile*)profile
       importedProfile:(AutoFillProfile*)importedProfile
    importedCreditCard:(CreditCard*)importedCreditCard {
  CHECK(profile);
  // Use initWithWindowNibPath: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibpath = [mac_util::MainAppBundle()
                       pathForResource:@"AutoFillDialog"
                       ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    observer_ = observer;
    profile_ = profile;
    importedProfile_ = importedProfile;
    importedCreditCard_ = importedCreditCard;

    // Use property here to trigger KVO binding.
    [self setAuxiliaryEnabled:profile_->GetPrefs()->GetBoolean(
        prefs::kAutoFillAuxiliaryProfilesEnabled)];

    // Do not use [NSMutableArray array] here; we need predictable destruction
    // which will be prevented by having a reference held by an autorelease
    // pool.

    // Initialize array of sub-controllers.
    addressFormViewControllers_.reset(
        [[NSMutableArray alloc] initWithCapacity:0]);

    // Initialize array of sub-controllers.
    creditCardFormViewControllers_.reset(
        [[NSMutableArray alloc] initWithCapacity:0]);

    NSString* defaultAddressLabel = base::SysWideToNSString(
        profile_->GetPrefs()->GetString(prefs::kAutoFillDefaultProfile));
    defaultAddressLabel_.reset([defaultAddressLabel retain]);

    NSString* defaultCreditCardLabel = base::SysWideToNSString(
        profile_->GetPrefs()->GetString(prefs::kAutoFillDefaultCreditCard));
    defaultCreditCardLabel_.reset([defaultCreditCardLabel retain]);
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
    scoped_nsobject<AutoFillAddressViewController> addressViewController(
        [[AutoFillAddressViewController alloc]
            initWithProfile:profiles_[i]
                 disclosure:disclosureState
                 controller:self]);
    [self willChangeValueForKey:@"addressLabels"];
    [addressFormViewControllers_.get() addObject:addressViewController];
    [self didChangeValueForKey:@"addressLabels"];

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[addressViewController view]
                positioned:NSWindowBelow relativeTo:insertionPoint];
    insertionPoint = [addressViewController view];
    [[addressViewController view] setFrameOrigin:NSMakePoint(0, 0)];
  }

  insertionPoint = creditCardSection_;
  for (size_t i = 0; i < creditCards_.size(); i++) {
    scoped_nsobject<AutoFillCreditCardViewController> creditCardViewController(
        [[AutoFillCreditCardViewController alloc]
            initWithCreditCard:creditCards_[i]
                    disclosure:NSOffState
                    controller:self]);
    [self willChangeValueForKey:@"creditCardLabels"];
    [creditCardFormViewControllers_.get() addObject:creditCardViewController];
    [self didChangeValueForKey:@"creditCardLabels"];

    // Embed the child view into our (owned by us) target view.
    [childView_ addSubview:[creditCardViewController view]
                positioned:NSWindowBelow relativeTo:insertionPoint];
    insertionPoint = [creditCardViewController view];
    [[creditCardViewController view] setFrameOrigin:NSMakePoint(0, 0)];
  }

  // During initialization the default accessors were returning faulty values
  // since the controller arrays weren't set up. Poke our observers.
  [self willChangeValueForKey:@"defaultAddressLabel"];
  [self didChangeValueForKey:@"defaultAddressLabel"];
  [self willChangeValueForKey:@"defaultCreditCardLabel"];
  [self didChangeValueForKey:@"defaultCreditCardLabel"];
}

- (void)onPersonalDataLoaded:(const std::vector<AutoFillProfile*>&)profiles
                 creditCards:(const std::vector<CreditCard*>&)creditCards {
  if (importedProfile_) {
      profiles_.push_back(*importedProfile_);
  }

  if (importedCreditCard_) {
      creditCards_.push_back(*importedCreditCard_);
  }

  // If we're not using imported data then use the data fetch from the web db.
  if (!importedProfile_ && !importedCreditCard_) {
    // Make local copy of |profiles|.
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter)
      profiles_.push_back(**iter);

    // Make local copy of |creditCards|.
    for (std::vector<CreditCard*>::const_iterator iter = creditCards.begin();
         iter != creditCards.end(); ++iter)
      creditCards_.push_back(**iter);
  }

  [self installChildViews];
}

@end
