// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
#pragma once

#import <Cocoa/Cocoa.h>
#include <vector>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/prefs/pref_member.h"

namespace AutoFillDialogControllerInternal {
class PersonalDataManagerObserver;
class PreferenceObserver;
}  // AutoFillDialogControllerInternal

@class AutoFillAddressSheetController;
@class AutoFillCreditCardSheetController;
@class AutoFillTableView;
class Profile;
@class WindowSizeAutosaver;

// A window controller for managing the AutoFill options dialog.
// Modelessly presents a dialog allowing the user to store
// personal address and credit card information.
@interface AutoFillDialogController : NSWindowController <NSTableViewDelegate> {
 @private
  // Outlet to the main NSTableView object listing both addresses and credit
  // cards with section headers for both.
  IBOutlet AutoFillTableView* tableView_;

  // Outlet to "Edit..." button.  Here for unit testing purposes.
  IBOutlet NSButton* editButton_;

  // This observer is passed in by the caller of the dialog.  When the dialog
  // is dismissed |observer_| is called with new values for the addresses and
  // credit cards.
  // Weak, not retained.
  AutoFillDialogObserver* observer_;

  // Reference to input parameter.
  // Weak, not retained.
  Profile* profile_;

  // Working list of input profiles.
  std::vector<AutoFillProfile> profiles_;

  // Working list of input credit cards.
  std::vector<CreditCard> creditCards_;

  // State of checkbox for enabling AutoFill in general.
  BooleanPrefMember autoFillEnabled_;

  // Whether AutoFill is controlled by configuration management.
  BOOL autoFillManaged_;

  // Whether AutoFill is managed and disabled.
  BOOL autoFillManagedAndDisabled_;

  // State of checkbox for enabling Mac Address Book integration.
  BooleanPrefMember auxiliaryEnabled_;

  // State for |itemIsSelected| property used in bindings for "Edit..." and
  // "Remove" buttons.
  BOOL itemIsSelected_;

  // State for |multipleSelected| property used in bindings for "Edit..."
  // button.
  BOOL multipleSelected_;

  // Utility object to save and restore dialog position.
  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;

  // Transient reference to address "Add" / "Edit" sheet for address
  // information.
  scoped_nsobject<AutoFillAddressSheetController> addressSheetController;

  // Transient reference to address "Add" / "Edit" sheet for credit card
  // information.
  scoped_nsobject<AutoFillCreditCardSheetController> creditCardSheetController;

  // Manages PersonalDataManager loading.
  scoped_ptr<AutoFillDialogControllerInternal::PersonalDataManagerObserver>
      personalDataManagerObserver_;

  // Watches for changes to the AutoFill enabled preference.
  scoped_ptr<AutoFillDialogControllerInternal::PreferenceObserver>
      preferenceObserver_;
}

// Property representing state of the AutoFill enabled preference.  Checkbox is
// bound to this in nib.  Also, enabled state of other controls are also bound
// to this property.
@property(nonatomic) BOOL autoFillEnabled;

// Property indicating whether AutoFill is under control of configuration
// management. The enabled state of the AutoFill enabled checkbox is bound to
// this property.
@property(nonatomic) BOOL autoFillManaged;

// Property that is true iff AutoFill is managed and disabled. The save button's
// enabled state is bound to this property.
@property(nonatomic) BOOL autoFillManagedAndDisabled;

// Property representing state of Address Book "me" card usage.  Checkbox is
// bound to this in nib.
@property(nonatomic) BOOL auxiliaryEnabled;

// Property representing selection state in |tableView_|.  Enabled state of
// edit and delete buttons are bound to this property.
@property(nonatomic) BOOL itemIsSelected;

// Property representing multiple selection state in |tableView_|.  Enabled
// state of edit button is bound to this property.
@property(nonatomic) BOOL multipleSelected;

// Main interface for displaying a modeless AutoFill dialog on screen.
// This class method creates a new |AutoFillDialogController| and runs it as a
// modeless dialog.  The controller autoreleases itself when the dialog is
// closed. |observer| can be NULL, but if it is, then no notification is sent
// during modifications to data.  If |observer| is non-NULL then its
// |OnAutoFillDialogApply| method is invoked with the new address and credit
// card information.
// |profile| must be non-NULL.
// AutoFill profile and credit card data is initialized from the
// |PersonalDataManager| that is associated with the input |profile|.
+ (void)showAutoFillDialogWithObserver:(AutoFillDialogObserver*)observer
                               profile:(Profile*)profile;

// IBActions for adding new items.
- (IBAction)addNewAddress:(id)sender;
- (IBAction)addNewCreditCard:(id)sender;

// IBAction for deleting an item.  |sender| is expected to be the "Remove"
// button.  The deletion acts on the selected item in either the address or
// credit card list.
- (IBAction)deleteSelection:(id)sender;

// IBAction for editing an item.  |sender| is expected to be the "Edit..."
// button.  The editing acts on the selected item in either the address or
// credit card list.
- (IBAction)editSelection:(id)sender;

// IBAction for opening the help link.  |sender| is expected to be the
// "About AutoFill" link.
- (IBAction)openHelp:(id)sender;

// NSTableView data source methods.
- (id)tableView:(NSTableView *)tableView
    objectValueForTableColumn:(NSTableColumn *)tableColumn
                          row:(NSInteger)rowIndex;

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView;

@end

// Interface exposed for unit testing.
@interface AutoFillDialogController (ExposedForUnitTests)
// Returns an instance of AutoFillDialogController.  See |-initWithObserver|
// for details about arguments.
// Note: controller is autoreleased when |-closeDialog| is called.
+ (AutoFillDialogController*)controllerWithObserver:
      (AutoFillDialogObserver*)observer
               profile:(Profile*)profile;

- (id)initWithObserver:(AutoFillDialogObserver*)observer
               profile:(Profile*)profile;
- (void)runModelessDialog;
- (void)closeDialog;
- (AutoFillAddressSheetController*)addressSheetController;
- (AutoFillCreditCardSheetController*)creditCardSheetController;
- (void)selectAddressAtIndex:(size_t)i;
- (void)selectCreditCardAtIndex:(size_t)i;
- (void)addSelectedAddressAtIndex:(size_t)i;
- (void)addSelectedCreditCardAtIndex:(size_t)i;
- (BOOL)editButtonEnabled;
- (std::vector<AutoFillProfile>&)profiles;
- (std::vector<CreditCard>&)creditCards;
@end

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_MAC_
