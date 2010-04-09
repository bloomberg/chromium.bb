// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/Foundation/GTMNSObject+KeyValueObserving.h"

// Private methods for the |AutoFillCreditCardViewController| class.
@interface AutoFillCreditCardViewController (PrivateMethods)
- (void)rebuildBillingAddressContents;
- (void)rebuildShippingAddressContents;
- (void)creditCardsChanged:(GTMKeyValueChangeNotification*)notification;
- (void)defaultChanged:(GTMKeyValueChangeNotification*)notification;
@end

@implementation AutoFillCreditCardViewController

@synthesize creditCardModel = creditCardModel_;
@synthesize billingAddressContents = billingAddressContents_;
@synthesize shippingAddressContents = shippingAddressContents_;

- (id)initWithCreditCard:(const CreditCard&)creditCard
              disclosure:(NSCellStateValue)disclosureState
              controller:(AutoFillDialogController*)parentController {
  self = [super initWithNibName:@"AutoFillCreditCardFormView"
                         bundle:mac_util::MainAppBundle()
                     disclosure:disclosureState];
  if (self) {
    // Pull in the view for initialization.
    [self view];

    // Create the model.  We use setter here for KVO.
    [self setCreditCardModel:[[[AutoFillCreditCardModel alloc]
        initWithCreditCard:creditCard] autorelease]];

    // We keep track of our parent controller for model-update purposes.
    parentController_ = parentController;

    // Setup initial state of popups.
    [self onAddressesChanged:self];

    [parentController_ gtm_addObserver:self
                            forKeyPath:@"creditCardLabels"
                              selector:@selector(creditCardsChanged:)
                              userInfo:nil
                               options:0];
    [parentController_ gtm_addObserver:self
                            forKeyPath:@"defaultCreditCardLabel"
                              selector:@selector(defaultChanged:)
                              userInfo:nil
                               options:0];
  }
  return self;
}

- (void)dealloc {
  [parentController_ gtm_removeObserver:self
                             forKeyPath:@"creditCardLabels"
                               selector:@selector(creditCardsChanged:)];
  [parentController_ gtm_removeObserver:self
                             forKeyPath:@"defaultCreditCardLabel"
                               selector:@selector(defaultChanged:)];
  [creditCardModel_ release];
  [billingAddressContents_ release];
  [shippingAddressContents_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [super awakeFromNib];

  // Turn menu autoenable off.  We manually govern this.
  [billingAddressPopup_ setAutoenablesItems:NO];
  [shippingAddressPopup_ setAutoenablesItems:NO];
}

- (IBAction)deleteCreditCard:(id)sender {
  [parentController_ deleteCreditCard:self];
}

- (IBAction)onAddressesChanged:(id)sender {
  [self rebuildBillingAddressContents];
  [self rebuildShippingAddressContents];
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  [creditCardModel_ copyModelToCreditCard:creditCard];

  // The model copies the shipping and billing addresses blindly.  We need
  // to clear the strings in the case that our special menus are in effect.
  if ([billingAddressPopup_ indexOfSelectedItem] <= 0)
    creditCard->set_billing_address(string16());
  if ([shippingAddressPopup_ indexOfSelectedItem] <= 0)
    creditCard->set_shipping_address(string16());
}

- (void)creditCardsChanged:(GTMKeyValueChangeNotification*)notification {
  [self willChangeValueForKey:@"canAlterDefault"];
  [self didChangeValueForKey:@"canAlterDefault"];
}

- (void)defaultChanged:(GTMKeyValueChangeNotification*)notification {
  [self willChangeValueForKey:@"isDefault"];
  [self didChangeValueForKey:@"isDefault"];
}

- (BOOL)canAlterDefault {
  return [[parentController_ creditCardLabels] count] > 1;
}

- (BOOL)isDefault {
  return [[creditCardModel_ label] isEqual:
      [parentController_ defaultCreditCardLabel]];
}

- (void)setIsDefault:(BOOL)def {
  [parentController_ setDefaultCreditCardLabel:
      def ? [creditCardModel_ label] : nil];
}

// Builds the |billingAddressContents_| array of strings from the list of
// addresses returned by the |parentController_| and additional UI string.
// Ensures that current selection is valid, if not reset it.
- (void)rebuildBillingAddressContents {
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

// Builds the |shippingAddressContents_| array of strings from the list of
// addresses returned by the |parentController_| and additional UI string.
// Ensures that current selection is valid, if not reset it.
- (void)rebuildShippingAddressContents {
  NSString* menuString = l10n_util::GetNSString(
      IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);

  // Build the menu array and set it.
  NSArray* addressStrings = [parentController_ addressLabels];
  NSArray* newArray = [[NSArray arrayWithObject:menuString]
      arrayByAddingObjectsFromArray:addressStrings];
  [self setShippingAddressContents:newArray];

  // If the addresses no longer contain our selected item, reset the selection.
  if ([addressStrings
        indexOfObject:[creditCardModel_ shippingAddress]] == NSNotFound) {
    [creditCardModel_ setShippingAddress:menuString];
  }
}

@end

