// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_sheet_controller_mac.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"

// Private methods for the |AutoFillCreditCardSheetController| class.
@interface AutoFillCreditCardSheetController (PrivateMethods)
- (void)buildBillingAddressContents;
- (void)buildExpirationMonthContents;
- (void)buildExpirationYearContents;
@end

@implementation AutoFillCreditCardSheetController

@synthesize creditCardModel = creditCardModel_;
@synthesize billingAddressContents = billingAddressContents_;
@synthesize expirationMonthContents = expirationMonthContents_;
@synthesize expirationYearContents = expirationYearContents_;

- (id)initWithCreditCard:(const CreditCard&)creditCard
                    mode:(AutoFillCreditCardMode)mode
              controller:(AutoFillDialogController*)parentController {
  NSString* nibPath = [mac_util::MainAppBundle()
                          pathForResource:@"AutoFillCreditCardSheet"
                                   ofType:@"nib"];
  self = [super initWithWindowNibPath:nibPath owner:self];
  if (self) {
    // Create the model.  We use setter here for KVO.
    [self setCreditCardModel:[[[AutoFillCreditCardModel alloc]
          initWithCreditCard:creditCard] autorelease]];

    // We keep track of our parent controller for model-update purposes.
    parentController_ = parentController;

    mode_ = mode;
  }
  return self;
}

- (void)dealloc {
  [creditCardModel_ release];
  [billingAddressContents_ release];
  [expirationMonthContents_ release];
  [expirationYearContents_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  // Setup initial state of popups.
  [self buildBillingAddressContents];
  [self buildExpirationMonthContents];
  [self buildExpirationYearContents];

  // Turn menu autoenable off.  We manually govern this.
  [billingAddressPopup_ setAutoenablesItems:NO];
  [expirationMonthPopup_ setAutoenablesItems:NO];
  [expirationYearPopup_ setAutoenablesItems:NO];

  // Set the caption based on the mode.
  NSString* caption = @"";
  if (mode_ == kAutoFillCreditCardAddMode)
    caption = l10n_util::GetNSString(IDS_AUTOFILL_ADD_CREDITCARD_CAPTION);
  else if (mode_ == kAutoFillCreditCardEditMode)
    caption = l10n_util::GetNSString(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION);
  else
    NOTREACHED();
  [caption_ setStringValue:caption];
}

- (IBAction)save:(id)sender {
  // Call |makeFirstResponder:| to commit pending text field edits.
  [[self window] makeFirstResponder:[self window]];

  [NSApp endSheet:[self window] returnCode:1];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window] returnCode:0];
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  // The model copies the popup values blindly.  We need to clear the strings
  // in the case that our special menus are in effect.
  if ([billingAddressPopup_ indexOfSelectedItem] <= 0)
    [creditCardModel_ setBillingAddress:@""];
  if ([expirationMonthPopup_ indexOfSelectedItem] <= 0)
    [creditCardModel_ setExpirationMonth:@""];
  if ([expirationYearPopup_ indexOfSelectedItem] <= 0)
    [creditCardModel_ setExpirationYear:@""];

  [creditCardModel_ copyModelToCreditCard:creditCard];
}

// Builds the |billingAddressContents_| array of strings from the list of
// addresses returned by the |parentController_| and additional UI string.
// Ensures that current selection is valid. If not, reset it.
- (void)buildBillingAddressContents {
  NSString* menuString = l10n_util::GetNSString(
      IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS);

  // Build the menu array and set it.
  NSArray* addressStrings = [parentController_ addressLabels];
  NSArray* newArray = [[NSArray arrayWithObject:menuString]
      arrayByAddingObjectsFromArray:addressStrings];
  [self setBillingAddressContents:newArray];

  // If the addresses no longer contain our selected item, reset the selection.
  if ([addressStrings
        indexOfObject:[creditCardModel_ billingAddress]] == NSNotFound) {
    [creditCardModel_ setBillingAddress:menuString];
  }

  // Disable first item in menu.  "Choose existing address" is a non-item.
  [[billingAddressPopup_ itemAtIndex:0] setEnabled:NO];
}

// Builds array of valid months.  Uses special @" " to indicate no selection.
- (void)buildExpirationMonthContents {
  NSArray* newArray = [NSArray arrayWithObjects:@" ",
      @"01", @"02", @"03", @"04", @"05", @"06",
      @"07", @"08", @"09", @"10", @"11", @"12", nil ];

  [self setExpirationMonthContents:newArray];

  // If the value from the model is not found in the array then set to the empty
  // item @" ".
  if ([newArray
        indexOfObject:[creditCardModel_ expirationMonth]] == NSNotFound) {
    [creditCardModel_ setExpirationMonth:@" "];
  }

  // Disable first item in menu.  @" " is a non-item.
  [[expirationMonthPopup_ itemAtIndex:0] setEnabled:NO];
}

// Builds array of valid years.  Uses special @" " to indicate no selection.
- (void)buildExpirationYearContents {
  NSArray* newArray = [NSArray arrayWithObjects:@" ",
      @"2010", @"2011", @"2012", @"2013", @"2014", @"2015",
      @"2016", @"2017", @"2018", @"2019", @"2020", nil ];

  [self setExpirationYearContents:newArray];

  // If the value from the model is not found in the array then set to the empty
  // item @" ".
  if ([newArray
        indexOfObject:[creditCardModel_ expirationYear]] == NSNotFound) {
    [creditCardModel_ setExpirationYear:@" "];
  }

  // Disable first item in menu.  @" " is a non-item.
  [[expirationYearPopup_ itemAtIndex:0] setEnabled:NO];
}

@end

