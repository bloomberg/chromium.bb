// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill_credit_card_edit_collection_view_controller.h"

#include "base/format_macros.h"
#import "base/ios/block_types.h"
#import "base/ios/weak_nsobject.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/settings/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

NSString* const kAutofillCreditCardEditCollectionViewId =
    @"kAutofillCreditCardEditCollectionViewId";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
  SectionIdentifierCopiedToChrome,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardholderName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeCopiedToChrome,
};

}  // namespace

@implementation AutofillCreditCardEditCollectionViewController {
  autofill::PersonalDataManager* _personalDataManager;  // weak
  autofill::CreditCard _creditCard;
}

#pragma mark - Initialization

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
               personalDataManager:(autofill::PersonalDataManager*)dataManager {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    DCHECK(dataManager);

    _personalDataManager = dataManager;
    _creditCard = creditCard;

    [self setCollectionViewAccessibilityIdentifier:
              kAutofillCreditCardEditCollectionViewId];
    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_CREDIT_CARD)];
    [self loadModel];
  }

  return self;
}

#pragma mark - SettingsRootCollectionViewController

- (void)editButtonPressed {
  // In the case of server cards, open the Payments editing page instead.
  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD ||
      _creditCard.record_type() == autofill::CreditCard::MASKED_SERVER_CARD) {
    GURL paymentsURL = autofill::payments::GetManageInstrumentsUrl(0);
    base::scoped_nsobject<OpenUrlCommand> command(
        [[OpenUrlCommand alloc] initWithURLFromChrome:paymentsURL]);
    [command setTag:IDC_CLOSE_SETTINGS_AND_OPEN_URL];
    [self chromeExecuteCommand:command];

    // Don't call [super editButtonPressed] because edit mode is not actually
    // entered in this case.
    return;
  }

  [super editButtonPressed];

  if (!self.editor.editing) {
    CollectionViewModel* model = self.collectionViewModel;
    NSInteger itemCount =
        [model numberOfItemsInSection:
                   [model sectionForSectionIdentifier:SectionIdentifierFields]];

    // Reads the values from the fields and updates the local copy of the
    // card accordingly.
    NSInteger section =
        [model sectionForSectionIdentifier:SectionIdentifierFields];
    for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      NSIndexPath* path =
          [NSIndexPath indexPathForItem:itemIndex inSection:section];
      AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
          [model itemAtIndexPath:path]);
      _creditCard.SetInfo(autofill::AutofillType(item.autofillType),
                          base::SysNSStringToUTF16(item.textFieldValue),
                          GetApplicationContext()->GetApplicationLocale());
    }

    _personalDataManager->UpdateCreditCard(_creditCard);
  }

  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.collectionViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]
         inSectionWithIdentifier:SectionIdentifierFields];
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  BOOL isEditing = self.editor.editing;

  [model addSectionWithIdentifier:SectionIdentifierFields];
  AutofillEditItem* cardholderNameitem = [[[AutofillEditItem alloc]
      initWithType:ItemTypeCardholderName] autorelease];
  cardholderNameitem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARDHOLDER);
  cardholderNameitem.textFieldValue = autofill::GetCreditCardName(
      _creditCard, GetApplicationContext()->GetApplicationLocale());
  cardholderNameitem.textFieldEnabled = isEditing;
  cardholderNameitem.autofillType = autofill::CREDIT_CARD_NAME_FULL;
  [model addItem:cardholderNameitem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Card number (PAN).
  AutofillEditItem* cardNumberitem =
      [[[AutofillEditItem alloc] initWithType:ItemTypeCardNumber] autorelease];
  cardNumberitem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER);
  // Never show full card number for Wallet cards, even if copied locally.
  cardNumberitem.textFieldValue =
      autofill::IsCreditCardLocal(_creditCard)
          ? base::SysUTF16ToNSString(_creditCard.number())
          : base::SysUTF16ToNSString(_creditCard.LastFourDigits());
  cardNumberitem.textFieldEnabled = isEditing;
  cardNumberitem.autofillType = autofill::CREDIT_CARD_NUMBER;
  [model addItem:cardNumberitem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Expiration month.
  AutofillEditItem* expirationMonthItem = [[[AutofillEditItem alloc]
      initWithType:ItemTypeExpirationMonth] autorelease];
  expirationMonthItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH);
  expirationMonthItem.textFieldValue =
      [NSString stringWithFormat:@"%02d", _creditCard.expiration_month()];
  expirationMonthItem.textFieldEnabled = isEditing;
  expirationMonthItem.autofillType = autofill::CREDIT_CARD_EXP_MONTH;
  [model addItem:expirationMonthItem
      toSectionWithIdentifier:SectionIdentifierFields];

  // Expiration year.
  AutofillEditItem* expirationYearItem = [[[AutofillEditItem alloc]
      initWithType:ItemTypeExpirationYear] autorelease];
  expirationYearItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR);
  expirationYearItem.textFieldValue =
      [NSString stringWithFormat:@"%04d", _creditCard.expiration_year()];
  expirationYearItem.textFieldEnabled = isEditing;
  expirationYearItem.autofillType = autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR;
  [model addItem:expirationYearItem
      toSectionWithIdentifier:SectionIdentifierFields];

  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD) {
    // Add CopiedToChrome cell in its own section.
    [model addSectionWithIdentifier:SectionIdentifierCopiedToChrome];
    CopiedToChromeItem* copiedToChromeItem = [[[CopiedToChromeItem alloc]
        initWithType:ItemTypeCopiedToChrome] autorelease];
    [model addItem:copiedToChromeItem
        toSectionWithIdentifier:SectionIdentifierCopiedToChrome];
  }
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  // The collection view needs to allow editing in order to respond to the Edit
  // button.
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Items in this collection view are not deletable, so should not be seen
  // as editable by the collection view.
  return NO;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  AutofillEditCell* textFieldCell = base::mac::ObjCCast<AutofillEditCell>(cell);
  textFieldCell.textField.delegate = self;
  switch (itemType) {
    case ItemTypeCardholderName:
      textFieldCell.textField.autocapitalizationType =
          UITextAutocapitalizationTypeWords;
      textFieldCell.textField.keyboardType = UIKeyboardTypeDefault;
      textFieldCell.textField.returnKeyType = UIReturnKeyNext;
      break;
    case ItemTypeCardNumber:
      textFieldCell.textField.autocapitalizationType =
          UITextAutocapitalizationTypeSentences;
      textFieldCell.textField.keyboardType = UIKeyboardTypeNumberPad;
      textFieldCell.textField.returnKeyType = UIReturnKeyNext;
      break;
    case ItemTypeExpirationMonth:
      textFieldCell.textField.autocapitalizationType =
          UITextAutocapitalizationTypeSentences;
      textFieldCell.textField.keyboardType = UIKeyboardTypeNumberPad;
      textFieldCell.textField.returnKeyType = UIReturnKeyNext;
      break;
    case ItemTypeExpirationYear:
      textFieldCell.textField.autocapitalizationType =
          UITextAutocapitalizationTypeSentences;
      textFieldCell.textField.keyboardType = UIKeyboardTypeNumberPad;
      textFieldCell.textField.returnKeyType = UIReturnKeyDone;
      break;
    case ItemTypeCopiedToChrome: {
      CopiedToChromeCell* copiedToChromeCell =
          base::mac::ObjCCastStrict<CopiedToChromeCell>(cell);
      [copiedToChromeCell.button addTarget:self
                                    action:@selector(buttonTapped:)
                          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - Actions

- (void)buttonTapped:(UIButton*)button {
  _personalDataManager->ResetFullServerCard(_creditCard.guid());

  // Reset the copy of the card data used for display immediately.
  _creditCard.set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
  _creditCard.SetNumber(_creditCard.LastFourDigits());
  [self reloadData];
}

@end
