// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill-inl.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_sheet_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_sheet_controller_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#import "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

namespace {

// Type for singleton object that contains the instance of the visible
// dialog.
typedef std::map<Profile*, AutoFillDialogController*> ProfileControllerMap;

static base::LazyInstance<ProfileControllerMap> g_profile_controller_map(
    base::LINKER_INITIALIZED);

}  // namespace

// Delegate protocol that needs to be in place for the AutoFillTableView's
// handling of delete and backspace keys.
@protocol DeleteKeyDelegate
- (IBAction)deleteSelection:(id)sender;
@end

// A subclass of NSTableView that allows for deleting selected elements using
// the delete or backspace keys.
@interface AutoFillTableView : NSTableView {
}
@end

@implementation AutoFillTableView

// We override the keyDown method to dispatch the |deleteSelection:| action
// when the user presses the delete or backspace keys.  Note a delegate must
// be present that conforms to the DeleteKeyDelegate protocol.
- (void)keyDown:(NSEvent *)event {
  id object = [self delegate];
  unichar c = [[event characters] characterAtIndex: 0];

  // If the user pressed delete and the delegate supports deleteSelection:
  if ((c == NSDeleteFunctionKey ||
       c == NSDeleteCharFunctionKey ||
       c == NSDeleteCharacter) &&
      [object respondsToSelector:@selector(deleteSelection:)]) {
    id <DeleteKeyDelegate> delegate = (id <DeleteKeyDelegate>) object;

    [delegate deleteSelection:self];
  } else {
    [super keyDown:event];
  }
}

@end

// Private interface.
@interface AutoFillDialogController (PrivateMethods)
// Save profiles and credit card information after user modification.
- (void)save;

// Asyncronous handler for when PersonalDataManager data loads.  The
// personal data manager notifies the dialog with this method when the
// data loading is complete and ready to be used.
- (void)onPersonalDataLoaded:(const std::vector<AutoFillProfile*>&)profiles
                 creditCards:(const std::vector<CreditCard*>&)creditCards;

// Asyncronous handler for when PersonalDataManager data changes.  The
// personal data manager notifies the dialog with this method when the
// data has changed.
- (void)onPersonalDataChanged:(const std::vector<AutoFillProfile*>&)profiles
                  creditCards:(const std::vector<CreditCard*>&)creditCards;

// Called upon changes to AutoFill preferences that should be reflected in the
// UI.
- (void)preferenceDidChange:(const std::string&)preferenceName;

// Adjust the selected index when underlying data changes.
// Selects the previous row if possible, else current row, else deselect all.
- (void) adjustSelectionOnDelete:(NSInteger)selectedRow;

// Adjust the selected index when underlying data changes.
// Selects the current row if possible, else previous row, else deselect all.
- (void) adjustSelectionOnReload:(NSInteger)selectedRow;

// Returns true if |row| is an index to a valid profile in |tableView_|, and
// false otherwise.
- (BOOL)isProfileRow:(NSInteger)row;

// Returns true if |row| is an index to the profile group row in |tableView_|,
// and false otherwise.
- (BOOL)isProfileGroupRow:(NSInteger)row;

// Returns true if |row| is an index to a valid credit card in |tableView_|, and
// false otherwise.
- (BOOL)isCreditCardRow:(NSInteger)row;

// Returns true if |row| is the index to the credit card group row in
// |tableView_|, and false otherwise.
- (BOOL)isCreditCardGroupRow:(NSInteger)row;

// Returns the index to |profiles_| of the corresponding |row| in |tableView_|.
- (size_t)profileIndexFromRow:(NSInteger)row;

// Returns the index to |creditCards_| of the corresponding |row| in
// |tableView_|.
- (size_t)creditCardIndexFromRow:(NSInteger)row;

// Returns the  |row| in |tableView_| that corresponds to the index |i| into
// |profiles_|.
- (NSInteger)rowFromProfileIndex:(size_t)i;

// Returns the  |row| in |tableView_| that corresponds to the index |i| into
// |creditCards_|.
- (NSInteger)rowFromCreditCardIndex:(size_t)row;

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

  // Notifies the observer that the PersonalDataManager data has changed.
  virtual void OnPersonalDataChanged();

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

// The data has been loaded, notify the controller.
void PersonalDataManagerObserver::OnPersonalDataLoaded() {
  [controller_ onPersonalDataLoaded:personal_data_manager_->web_profiles()
                        creditCards:personal_data_manager_->credit_cards()];
}

// The data has changed, notify the controller.
void PersonalDataManagerObserver::OnPersonalDataChanged() {
  [controller_ onPersonalDataChanged:personal_data_manager_->web_profiles()
                         creditCards:personal_data_manager_->credit_cards()];
}

// Bridges preference changed notifications to the dialog controller.
class PreferenceObserver : public NotificationObserver {
 public:
  explicit PreferenceObserver(AutoFillDialogController* controller)
      : controller_(controller) {}

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED) {
      const std::string* pref = Details<std::string>(details).ptr();
      if (pref) {
        [controller_ preferenceDidChange:*pref];
      }
    }
  }

 private:
  AutoFillDialogController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceObserver);
};

}  // namespace AutoFillDialogControllerInternal

@implementation AutoFillDialogController

@synthesize autoFillManaged = autoFillManaged_;
@synthesize autoFillManagedAndDisabled = autoFillManagedAndDisabled_;
@synthesize itemIsSelected = itemIsSelected_;
@synthesize multipleSelected = multipleSelected_;

+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                               profile:(Profile*)profile {
  AutoFillDialogController* controller =
      [AutoFillDialogController controllerWithObserver:observer
                                               profile:profile];
  [controller runModelessDialog];
}

- (void)awakeFromNib {
  PersonalDataManager* personal_data_manager =
      profile_->GetPersonalDataManager();
  DCHECK(personal_data_manager);

  if (personal_data_manager->IsDataLoaded()) {
    // |personalDataManager| data is loaded, we can proceed with the contents.
    [self onPersonalDataLoaded:personal_data_manager->web_profiles()
                   creditCards:personal_data_manager->credit_cards()];
  }

  // Register as listener to listen to subsequent data change notifications.
  personalDataManagerObserver_.reset(
      new AutoFillDialogControllerInternal::PersonalDataManagerObserver(
          self, personal_data_manager, profile_));
  personal_data_manager->SetObserver(personalDataManagerObserver_.get());

  // Explicitly load the data in the table before window displays to avoid
  // nasty flicker as tables update.
  [tableView_ reloadData];

  // Set up edit when double-clicking on a table row.
  [tableView_ setDoubleAction:@selector(editSelection:)];
}

// NSWindow Delegate callback.  When the window closes the controller can
// be released.
- (void)windowWillClose:(NSNotification *)notification {
  [tableView_ setDataSource:nil];
  [tableView_ setDelegate:nil];
  [self autorelease];

  // Remove ourself from the map.
  ProfileControllerMap* map = g_profile_controller_map.Pointer();
  ProfileControllerMap::iterator it = map->find(profile_);
  if (it != map->end()) {
    map->erase(it);
  }
}

// Invokes the "Add" sheet for address information.  If user saves then the new
// information is added to |profiles_| in |addressAddDidEnd:| method.
- (IBAction)addNewAddress:(id)sender {
  DCHECK(!addressSheetController.get());

  // Create a new default address.
  AutoFillProfile newAddress;

  // Create a new address sheet controller in "Add" mode.
  addressSheetController.reset(
      [[AutoFillAddressSheetController alloc]
          initWithProfile:newAddress
                     mode:kAutoFillAddressAddMode]);

  // Show the sheet.
  [NSApp beginSheet:[addressSheetController window]
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(addressAddDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
}

// Invokes the "Add" sheet for credit card information.  If user saves then the
// new information is added to |creditCards_| in |creditCardAddDidEnd:| method.
- (IBAction)addNewCreditCard:(id)sender {
  DCHECK(!creditCardSheetController.get());

  // Create a new default credit card.
  CreditCard newCreditCard;

  // Create a new address sheet controller in "Add" mode.
  creditCardSheetController.reset(
      [[AutoFillCreditCardSheetController alloc]
          initWithCreditCard:newCreditCard
                        mode:kAutoFillCreditCardAddMode]);

  // Show the sheet.
  [NSApp beginSheet:[creditCardSheetController window]
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(creditCardAddDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
}

// Add address sheet was dismissed.  Non-zero |returnCode| indicates a save.
- (void)addressAddDidEnd:(NSWindow*)sheet
              returnCode:(int)returnCode
             contextInfo:(void*)contextInfo {
  DCHECK(!contextInfo);

  if (returnCode) {
    // Create a new address and save it to the |profiles_| list.
    AutoFillProfile newAddress;
    [addressSheetController copyModelToProfile:&newAddress];
    if (!newAddress.IsEmpty() && !FindByContents(profiles_, newAddress)) {
      profiles_.push_back(newAddress);

      // Saving will save to the PDM and the table will refresh when PDM sends
      // notification that the underlying model has changed.
      [self save];

      // Update the selection to the newly added item.
      NSInteger row = [self rowFromProfileIndex:profiles_.size() - 1];
      [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:row]
              byExtendingSelection:NO];
    }
  }
  [sheet orderOut:self];
  addressSheetController.reset(nil);
}

// Add credit card sheet was dismissed.  Non-zero |returnCode| indicates a save.
- (void)creditCardAddDidEnd:(NSWindow*)sheet
                 returnCode:(int)returnCode
                contextInfo:(void*)contextInfo {
  DCHECK(!contextInfo);

  if (returnCode) {
    // Create a new credit card and save it to the |creditCards_| list.
    CreditCard newCreditCard;
    [creditCardSheetController copyModelToCreditCard:&newCreditCard];
    if (!newCreditCard.IsEmpty() &&
        !FindByContents(creditCards_, newCreditCard)) {
      creditCards_.push_back(newCreditCard);

      // Saving will save to the PDM and the table will refresh when PDM sends
      // notification that the underlying model has changed.
      [self save];

      // Update the selection to the newly added item.
      NSInteger row = [self rowFromCreditCardIndex:creditCards_.size() - 1];
      [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:row]
              byExtendingSelection:NO];
    }
  }
  [sheet orderOut:self];
  creditCardSheetController.reset(nil);
}

// Deletes selected items; either addresses, credit cards, or a mixture of the
// two depending on the items selected.
- (IBAction)deleteSelection:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  NSInteger selectedRow = [tableView_ selectedRow];

  // Loop through from last to first deleting selected items as we go.
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    // We keep track of the "top most" selection in the list so we know where
    // to set new selection below.
    selectedRow = i;

    if ([self isProfileRow:i]) {
      profiles_.erase(
          profiles_.begin() + [self profileIndexFromRow:i]);
    } else if ([self isCreditCardRow:i]) {
      creditCards_.erase(
          creditCards_.begin() + [self creditCardIndexFromRow:i]);
    }
  }

  // Select the previous row if possible, else current row, else deselect all.
  [self adjustSelectionOnDelete:selectedRow];

  // Saving will save to the PDM and the table will refresh when PDM sends
  // notification that the underlying model has changed.
  [self save];
}

// Edits the selected item, either address or credit card depending on the item
// selected.
- (IBAction)editSelection:(id)sender {
  NSInteger selectedRow = [tableView_ selectedRow];
  if ([self isProfileRow:selectedRow]) {
    if (!addressSheetController.get()) {
      int i = [self profileIndexFromRow:selectedRow];

      // Create a new address sheet controller in "Edit" mode.
      addressSheetController.reset(
          [[AutoFillAddressSheetController alloc]
              initWithProfile:profiles_[i]
                         mode:kAutoFillAddressEditMode]);

      // Show the sheet.
      [NSApp beginSheet:[addressSheetController window]
         modalForWindow:[self window]
          modalDelegate:self
         didEndSelector:@selector(addressEditDidEnd:returnCode:contextInfo:)
            contextInfo:&profiles_[i]];
    }
  } else if ([self isCreditCardRow:selectedRow]) {
    if (!creditCardSheetController.get()) {
      int i = [self creditCardIndexFromRow:selectedRow];

      // Create a new credit card sheet controller in "Edit" mode.
      creditCardSheetController.reset(
          [[AutoFillCreditCardSheetController alloc]
              initWithCreditCard:creditCards_[i]
                            mode:kAutoFillCreditCardEditMode]);

      // Show the sheet.
      [NSApp beginSheet:[creditCardSheetController window]
         modalForWindow:[self window]
          modalDelegate:self
         didEndSelector:@selector(creditCardEditDidEnd:returnCode:contextInfo:)
            contextInfo:&creditCards_[i]];
    }
  }
}

// Navigates to the AutoFill help url.
- (IBAction)openHelp:(id)sender {
  Browser* browser = BrowserList::GetLastActive();

  if (!browser || !browser->GetSelectedTabContents())
    browser = Browser::Create(profile_);
  browser->OpenAutoFillHelpTabAndActivate();
}

// Edit address sheet was dismissed.  Non-zero |returnCode| indicates a save.
- (void)addressEditDidEnd:(NSWindow *)sheet
               returnCode:(int)returnCode
              contextInfo:(void *)contextInfo {
  DCHECK(contextInfo != NULL);
  if (returnCode) {
    AutoFillProfile* profile = static_cast<AutoFillProfile*>(contextInfo);
    [addressSheetController copyModelToProfile:profile];

    if (profile->IsEmpty())
      [tableView_ deselectAll:self];
    profiles_.erase(
        std::remove_if(profiles_.begin(), profiles_.end(),
                       std::mem_fun_ref(&AutoFillProfile::IsEmpty)),
        profiles_.end());

    // Saving will save to the PDM and the table will refresh when PDM sends
    // notification that the underlying model has changed.
    [self save];
  }
  [sheet orderOut:self];
  addressSheetController.reset(nil);
}

// Edit credit card sheet was dismissed.  Non-zero |returnCode| indicates a
// save.
- (void)creditCardEditDidEnd:(NSWindow *)sheet
                  returnCode:(int)returnCode
                 contextInfo:(void *)contextInfo {
  DCHECK(contextInfo != NULL);
  if (returnCode) {
    CreditCard* creditCard = static_cast<CreditCard*>(contextInfo);
    [creditCardSheetController copyModelToCreditCard:creditCard];

    if (creditCard->IsEmpty())
      [tableView_ deselectAll:self];
    creditCards_.erase(
        std::remove_if(
            creditCards_.begin(), creditCards_.end(),
            std::mem_fun_ref(&CreditCard::IsEmpty)),
        creditCards_.end());

    // Saving will save to the PDM and the table will refresh when PDM sends
    // notification that the underlying model has changed.
    [self save];
  }
  [sheet orderOut:self];
  creditCardSheetController.reset(nil);
}

// NSTableView Delegate method.
- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row {
  if ([self isProfileGroupRow:row] || [self isCreditCardGroupRow:row])
    return YES;
  return NO;
}

// NSTableView Delegate method.
- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row {
  return [self isProfileRow:row] || [self isCreditCardRow:row];
}

// NSTableView Delegate method.
- (id)tableView:(NSTableView *)tableView
  objectValueForTableColumn:(NSTableColumn *)tableColumn
                        row:(NSInteger)row {
  if ([[tableColumn identifier] isEqualToString:@"Spacer"])
    return @"";

  // Check that we're initialized before supplying data.
  if (tableView != tableView_)
    return @"";

  // Section label.
  if ([self isProfileGroupRow:row]) {
    if ([[tableColumn identifier] isEqualToString:@"Summary"])
      return l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_GROUP_NAME);
    else
      return @"";
  }

  if (row < 0)
    return @"";

  // Data row.
  if ([self isProfileRow:row]) {
    if ([[tableColumn identifier] isEqualToString:@"Summary"]) {
      return SysUTF16ToNSString(
          profiles_[[self profileIndexFromRow:row]].Label());
    }

    return @"";
  }

  // Section label.
  if ([self isCreditCardGroupRow:row]) {
    if ([[tableColumn identifier] isEqualToString:@"Summary"])
      return l10n_util::GetNSString(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME);
    else
      return @"";
  }

  // Data row.
  if ([self isCreditCardRow:row]) {
    if ([[tableColumn identifier] isEqualToString:@"Summary"]) {
      return SysUTF16ToNSString(
          creditCards_[
              [self creditCardIndexFromRow:row]].PreviewSummary());
    }

    return @"";
  }

  return @"";
}

// We implement this delegate method to update our |itemIsSelected| and
// |multipleSelected| properties.
// The "Edit..." and "Remove" buttons' enabled state depends on having a
// valid selection in the table.  The "Edit..." button depends on having
// exactly one item selected.
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  if ([tableView_ selectedRow] >= 0)
    [self setItemIsSelected:YES];
  else
    [self setItemIsSelected:NO];

  [self setMultipleSelected:([[tableView_ selectedRowIndexes] count] > 1UL)];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
  if (tableView == tableView_) {
    // 1 section header, the profiles, 1 section header, the credit cards.
    return 1 + profiles_.size() + 1 + creditCards_.size();
  }

  return 0;
}

// Accessor for |autoFillEnabled| preference state.  Note: a checkbox in Nib
// is bound to this via KVO.
- (BOOL)autoFillEnabled {
  return autoFillEnabled_.GetValue();
}

// Setter for |autoFillEnabled| preference state.
- (void)setAutoFillEnabled:(BOOL)value {
  autoFillEnabled_.SetValueIfNotManaged(value ? true : false);
}

// Accessor for |auxiliaryEnabled| preference state.  Note: a checkbox in Nib
// is bound to this via KVO.
- (BOOL)auxiliaryEnabled {
  return auxiliaryEnabled_.GetValue();
}

// Setter for |auxiliaryEnabled| preference state.
- (void)setAuxiliaryEnabled:(BOOL)value {
  if ([self autoFillEnabled])
    auxiliaryEnabled_.SetValueIfNotManaged(value ? true : false);
}

@end

@implementation AutoFillDialogController (ExposedForUnitTests)

+ (AutoFillDialogController*)
    controllerWithObserver:(AutoFillDialogObserver*)observer
                   profile:(Profile*)profile {
  profile = profile->GetOriginalProfile();

  ProfileControllerMap* map = g_profile_controller_map.Pointer();
  DCHECK(map != NULL);
  ProfileControllerMap::iterator it = map->find(profile);
  if (it == map->end()) {
    // We should have exactly 1 or 0 entry in the map, no more.  That is,
    // only one profile can have the AutoFill dialog up at a time.
    DCHECK_EQ(map->size(), 0U);

  // Deallocation is done upon window close.  See |windowWillClose:|.
  AutoFillDialogController* controller =
      [[self alloc] initWithObserver:observer profile:profile];
    it = map->insert(std::make_pair(profile, controller)).first;
  }

  return it->second;
}


// This is the designated initializer for this class.
// |profiles| are non-retained immutable list of AutoFill profiles.
// |creditCards| are non-retained immutable list of credit card info.
- (id)initWithObserver:(AutoFillDialogObserver*)observer
               profile:(Profile*)profile {
  DCHECK(profile);
  // Use initWithWindowNibPath: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibpath = [base::mac::MainAppBundle()
                       pathForResource:@"AutoFillDialog"
                       ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    // Initialize member variables based on input.
    observer_ = observer;
    profile_ = profile;

    // Initialize the preference observer and watch kAutoFillEnabled.
    preferenceObserver_.reset(
        new AutoFillDialogControllerInternal::PreferenceObserver(self));
    autoFillEnabled_.Init(prefs::kAutoFillEnabled, profile_->GetPrefs(),
                          preferenceObserver_.get());

    // Call |preferenceDidChange| in order to initialize UI state of the
    // checkbox.
    [self preferenceDidChange:prefs::kAutoFillEnabled];

    // Initialize the preference observer and watch
    // kAutoFillAuxiliaryProfilesEnabled.
    auxiliaryEnabled_.Init(prefs::kAutoFillAuxiliaryProfilesEnabled,
                           profile_->GetPrefs(),
                           preferenceObserver_.get());

    // Call |preferenceDidChange| in order to initialize UI state of the
    // checkbox.
    [self preferenceDidChange:prefs::kAutoFillAuxiliaryProfilesEnabled];

    // Do not use [NSMutableArray array] here; we need predictable destruction
    // which will be prevented by having a reference held by an autorelease
    // pool.
  }
  return self;
}

// Run modeless.
- (void)runModelessDialog {
  // Use stored window geometry if it exists.
  if (g_browser_process && g_browser_process->local_state()) {
    sizeSaver_.reset([[WindowSizeAutosaver alloc]
        initWithWindow:[self window]
           prefService:g_browser_process->local_state()
                  path:prefs::kAutoFillDialogPlacement]);
  }

  [self showWindow:nil];
}

// Close the dialog.
- (void)closeDialog {
  [[self window] performClose:self];
}

- (AutoFillAddressSheetController*)addressSheetController {
  return addressSheetController.get();
}

- (AutoFillCreditCardSheetController*)creditCardSheetController {
  return creditCardSheetController.get();
}

- (void)selectAddressAtIndex:(size_t)i {
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:
                                   [self rowFromProfileIndex:i]]
          byExtendingSelection:NO];
}

- (void)selectCreditCardAtIndex:(size_t)i {
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:
                                   [self rowFromCreditCardIndex:i]]
          byExtendingSelection:NO];
}

- (void)addSelectedAddressAtIndex:(size_t)i {
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:
                                   [self rowFromProfileIndex:i]]
          byExtendingSelection:YES];
}

- (void)addSelectedCreditCardAtIndex:(size_t)i {
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:
                                   [self rowFromCreditCardIndex:i]]
          byExtendingSelection:YES];
}

- (BOOL)editButtonEnabled {
  return [editButton_ isEnabled];
}

- (std::vector<AutoFillProfile>&)profiles {
  return profiles_;
}

- (std::vector<CreditCard>&)creditCards {
  return creditCards_;
}

@end

@implementation AutoFillDialogController (PrivateMethods)

// Called when the user modifies the profiles or credit card information.
- (void)save {
  // If we have an |observer_| then communicate the changes back, unless
  // AutoFill has been disabled through policy in the mean time.
  if (observer_ && !autoFillManagedAndDisabled_) {
    // Make a working copy of profiles.  |OnAutoFillDialogApply| can mutate
    // |profiles_|.
    std::vector<AutoFillProfile> profiles = profiles_;

    // Make a working copy of credit cards.  |OnAutoFillDialogApply| can mutate
    // |creditCards_|.
    std::vector<CreditCard> creditCards = creditCards_;

    observer_->OnAutoFillDialogApply(&profiles, &creditCards);
  }
}

- (void)onPersonalDataLoaded:(const std::vector<AutoFillProfile*>&)profiles
                 creditCards:(const std::vector<CreditCard*>&)creditCards {
  [self onPersonalDataChanged:profiles creditCards:creditCards];
}

- (void)onPersonalDataChanged:(const std::vector<AutoFillProfile*>&)profiles
                  creditCards:(const std::vector<CreditCard*>&)creditCards {
  // Make local copy of |profiles|.
  profiles_.clear();
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter)
    profiles_.push_back(**iter);

  // Make local copy of |creditCards|.
  creditCards_.clear();
  for (std::vector<CreditCard*>::const_iterator iter = creditCards.begin();
       iter != creditCards.end(); ++iter)
    creditCards_.push_back(**iter);

  [self adjustSelectionOnReload:[tableView_ selectedRow]];
  [tableView_ reloadData];
}

- (void)preferenceDidChange:(const std::string&)preferenceName {
  if (preferenceName == prefs::kAutoFillEnabled) {
    [self setAutoFillEnabled:autoFillEnabled_.GetValue()];
    [self setAutoFillManaged:autoFillEnabled_.IsManaged()];
    [self setAutoFillManagedAndDisabled:
        autoFillEnabled_.IsManaged() && !autoFillEnabled_.GetValue()];
  } else if (preferenceName == prefs::kAutoFillAuxiliaryProfilesEnabled) {
    [self setAuxiliaryEnabled:auxiliaryEnabled_.GetValue()];
  } else {
    NOTREACHED();
  }
}

- (void) adjustSelectionOnDelete:(NSInteger)selectedRow {
  if ([self tableView:tableView_ shouldSelectRow:selectedRow-1]) {
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow-1]
            byExtendingSelection:NO];
  } else if ([self tableView:tableView_ shouldSelectRow:selectedRow]) {
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow]
            byExtendingSelection:NO];
  } else {
    [tableView_ deselectAll:self];
  }
}

- (void) adjustSelectionOnReload:(NSInteger)selectedRow {
  if ([self tableView:tableView_ shouldSelectRow:selectedRow]) {
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow]
            byExtendingSelection:NO];
  } else if ([self tableView:tableView_ shouldSelectRow:selectedRow-1]) {
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow-1]
            byExtendingSelection:NO];
  } else {
    [tableView_ deselectAll:self];
  }
}

- (BOOL)isProfileRow:(NSInteger)row {
  if (row > 0 && static_cast<size_t>(row) <= profiles_.size())
    return YES;
  return NO;
}

- (BOOL)isProfileGroupRow:(NSInteger)row {
  if (row == 0)
    return YES;
  return NO;
}

- (BOOL)isCreditCardRow:(NSInteger)row {
  if (row > 0 &&
      static_cast<size_t>(row) >= profiles_.size() + 2 &&
      static_cast<size_t>(row) <= profiles_.size() + creditCards_.size() + 1)
    return YES;
  return NO;
}

- (BOOL)isCreditCardGroupRow:(NSInteger)row {
  if (row > 0 && static_cast<size_t>(row) == profiles_.size() + 1)
    return YES;
  return NO;
}

- (size_t)profileIndexFromRow:(NSInteger)row {
  DCHECK([self isProfileRow:row]);
  return static_cast<size_t>(row) - 1;
}

- (size_t)creditCardIndexFromRow:(NSInteger)row {
  DCHECK([self isCreditCardRow:row]);
  return static_cast<size_t>(row) - (profiles_.size() + 2);
}

- (NSInteger)rowFromProfileIndex:(size_t)i {
  return 1 + i;
}

- (NSInteger)rowFromCreditCardIndex:(size_t)i {
  return 1 + profiles_.size() + 1 + i;
}

@end

// An NSValueTransformer subclass for use in validation of phone number
// fields.  Transforms an invalid phone number string into a warning image.
// This data transformer is used in the credit card sheet for invalid phone and
// fax numbers.
@interface InvalidPhoneTransformer : NSValueTransformer {
}
@end

@implementation InvalidPhoneTransformer
+ (Class)transformedValueClass {
  return [NSImage class];
}

+ (BOOL)allowsReverseTransformation {
  return NO;
}

- (id)transformedValue:(id)string {
  NSImage* image = nil;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // We display no validation icon when input has not yet been entered.
  if (string == nil || [string length] == 0)
    return nil;

  // If we have input then display alert icon if we have an invalid number.
  if (string != nil && [string length] != 0) {
    // TODO(dhollowa): Using SetInfo() call to validate phone number.  Should
    // have explicit validation method.  More robust validation is needed as
    // well eventually.
    AutoFillProfile profile;
    profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                    base::SysNSStringToUTF16(string));
    if (profile.GetFieldText(AutofillType(PHONE_HOME_WHOLE_NUMBER)).empty()) {
      image = rb.GetNativeImageNamed(IDR_INPUT_ALERT);
      DCHECK(image);
      return image;
    }
  }

  // No alert icon, so must be valid input.
  if (!image) {
    image = rb.GetNativeImageNamed(IDR_INPUT_GOOD);
    DCHECK(image);
    return image;
  }

  return nil;
}

@end
