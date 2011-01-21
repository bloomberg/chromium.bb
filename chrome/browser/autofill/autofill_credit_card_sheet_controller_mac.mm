// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_sheet_controller_mac.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// Interface exposed for unit testing.
@implementation AutoFillCreditCardSheetController (ExposedForUnitTests)
- (NSTextField*)creditCardNumberField {
  return creditCardNumberField_;
}
@end

// Private methods for the |AutoFillCreditCardSheetController| class.
@interface AutoFillCreditCardSheetController (PrivateMethods)
- (void)buildExpirationMonthContents;
- (void)buildExpirationYearContents;
@end

@implementation AutoFillCreditCardSheetController

@synthesize creditCardModel = creditCardModel_;
@synthesize expirationMonthContents = expirationMonthContents_;
@synthesize expirationYearContents = expirationYearContents_;

- (id)initWithCreditCard:(const CreditCard&)creditCard
                    mode:(AutoFillCreditCardMode)mode {
  NSString* nibPath = [base::mac::MainAppBundle()
                          pathForResource:@"AutoFillCreditCardSheet"
                                   ofType:@"nib"];
  self = [super initWithWindowNibPath:nibPath owner:self];
  if (self) {
    // Create the model.  We use setter here for KVO.
    [self setCreditCardModel:[[[AutoFillCreditCardModel alloc]
          initWithCreditCard:creditCard] autorelease]];

    mode_ = mode;
  }
  return self;
}

- (void)dealloc {
  [creditCardModel_ release];
  [expirationMonthContents_ release];
  [expirationYearContents_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  // Setup initial state of popups.
  [self buildExpirationMonthContents];
  [self buildExpirationYearContents];

  // Turn menu autoenable off.  We manually govern this.
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
  if ([expirationMonthPopup_ indexOfSelectedItem] <= 0)
    [creditCardModel_ setExpirationMonth:@""];
  if ([expirationYearPopup_ indexOfSelectedItem] <= 0)
    [creditCardModel_ setExpirationYear:@""];

  [creditCardModel_ copyModelToCreditCard:creditCard];
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

