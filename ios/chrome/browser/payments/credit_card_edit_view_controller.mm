// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/credit_card_edit_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_edit_view_controller+internal.h"
#import "ios/chrome/browser/payments/payment_request_edit_view_controller_actions.h"
#import "ios/chrome/browser/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kCreditCardEditCollectionViewId =
    @"kCreditCardEditCollectionViewId";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAcceptedMethods = kSectionIdentifierEnumStart,
  SectionIdentifierCardSummary,
  SectionIdentifierBillingAddress,
  SectionIdentifierSaveCard,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAcceptedMethods = kItemTypeEnumStart,
  ItemTypeCardSummary,
  ItemTypeBillingAddress,
  ItemTypeSaveCard,
};

}  // namespace

@interface CreditCardEditViewController () {
  NSArray<EditorField*>* _fields;

  // Indicates whether the credit card being created should be saved locally.
  BOOL _saveCreditCard;
}

@end

@implementation CreditCardEditViewController

@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;
@synthesize billingAddressGUID = _billingAddressGUID;

#pragma mark - Initialization

- (instancetype)init {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _saveCreditCard = YES;

    // Set up leading (cancel) button.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(onCancel)];
    [cancelButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                                forState:UIControlStateDisabled];
    [cancelButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CANCEL)];
    [self navigationItem].leftBarButtonItem = cancelButton;

    // Set up trailing (done) button.
    UIBarButtonItem* doneButton =
        [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_DONE)
                                         style:UIBarButtonItemStylePlain
                                        target:nil
                                        action:@selector(onDone)];
    [doneButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                              forState:UIControlStateDisabled];
    [doneButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_DONE)];
    [self navigationItem].rightBarButtonItem = doneButton;
  }

  return self;
}

- (void)setDataSource:(id<CreditCardEditViewControllerDataSource>)dataSource {
  [super setDataSource:dataSource];
  _dataSource = dataSource;
  _fields = [dataSource editorFields];
}

- (BOOL)validateForm {
  if (![super validateForm])
    return NO;

  // TODO(crbug.com/602666): Uncomment the following when billing address
  // selection UI is ready.
  // Validate the billing address GUID.
  //  NSString* errorMessage =
  //      !_billingAddressGUID ? l10n_util::GetNSString(
  //                             IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE)
  //                           : nil;
  //  [self addOrRemoveErrorMessage:errorMessage
  //        inSectionWithIdentifier:SectionIdentifierBillingAddress];
  //  if (errorMessage) {
  //    return NO;
  //  }

  return YES;
}

#pragma mark - PaymentRequestEditViewControllerActions methods

- (void)onCancel {
  [_delegate creditCardEditViewControllerDidCancel:self];
}

- (void)onDone {
  if (![self validateForm])
    return;

  [_delegate creditCardEditViewController:self
                   didFinishEditingFields:_fields
                         billingAddressID:_billingAddressGUID
                           saveCreditCard:_saveCreditCard];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];

  // If editing a credit card, set the card type icon (e.g. "Visa").
  if (_dataSource.state == CreditCardEditViewControllerStateEdit) {
    for (EditorField* field in _fields) {
      AutofillEditItem* item = field.item;
      if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
        item.cardTypeIcon =
            [_dataSource cardTypeIconFromCardNumber:field.value];
      }
    }
  }
}

- (void)loadHeaderItems {
  [super loadHeaderItems];
  CollectionViewModel* model = self.collectionViewModel;

  // Server card summary section.
  CollectionViewItem* serverCardSummaryItem =
      [_dataSource serverCardSummaryItem];
  if (serverCardSummaryItem) {
    [model addSectionWithIdentifier:SectionIdentifierCardSummary];
    serverCardSummaryItem.type = ItemTypeCardSummary;
    [model addItem:serverCardSummaryItem
        toSectionWithIdentifier:SectionIdentifierCardSummary];
  }

  // Accepted payment methods section.
  CollectionViewItem* acceptedMethodsItem =
      [_dataSource acceptedPaymentMethodsItem];
  if (acceptedMethodsItem) {
    [model addSectionWithIdentifier:SectionIdentifierAcceptedMethods];
    acceptedMethodsItem.type = ItemTypeAcceptedMethods;
    [model addItem:acceptedMethodsItem
        toSectionWithIdentifier:SectionIdentifierAcceptedMethods];
  }
}

- (void)loadFooterItems {
  CollectionViewModel* model = self.collectionViewModel;

  // Billing Address section.
  [model addSectionWithIdentifier:SectionIdentifierBillingAddress];
  CollectionViewDetailItem* billingAddressItem =
      [[CollectionViewDetailItem alloc] initWithType:ItemTypeBillingAddress];
  billingAddressItem.text = [NSString
      stringWithFormat:@"%@*",
                       l10n_util::GetNSString(IDS_PAYMENTS_BILLING_ADDRESS)];
  billingAddressItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  [model addItem:billingAddressItem
      toSectionWithIdentifier:SectionIdentifierBillingAddress];

  if (_billingAddressGUID) {
    billingAddressItem.detailText =
        [_dataSource billingAddressLabelForProfileWithGUID:_billingAddressGUID];
  }

  // "Save card" section. Visible only when creating a card.
  if (_dataSource.state == CreditCardEditViewControllerStateCreate) {
    [model addSectionWithIdentifier:SectionIdentifierSaveCard];
    CollectionViewSwitchItem* saveCardItem =
        [[CollectionViewSwitchItem alloc] initWithType:ItemTypeSaveCard];
    saveCardItem.text =
        l10n_util::GetNSString(IDS_PAYMENTS_SAVE_CARD_TO_DEVICE_CHECKBOX);
    saveCardItem.on = _saveCreditCard;
    [model addItem:saveCardItem
        toSectionWithIdentifier:SectionIdentifierSaveCard];
  }

  [super loadFooterItems];
}

#pragma mark - UITextFieldDelegate

// This method is called as the text is being typed in, pasted, or deleted. Asks
// the delegate if the text should be changed. Should always return YES. During
// typing/pasting text, |newText| contains one or more new characters. When user
// deletes text, |newText| is empty. |range| is the range of characters to be
// replaced.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  NSIndexPath* indexPath = [self indexPathForCurrentTextField];
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);

  // If the user is typing in the credit card number field, update the card type
  // icon (e.g. "Visa") to reflect the number being typed.
  if (item.autofillUIType == AutofillUITypeCreditCardNumber) {
    // Obtain the text being typed.
    NSString* updatedText =
        [textField.text stringByReplacingCharactersInRange:range
                                                withString:newText];
    item.cardTypeIcon = [_dataSource cardTypeIconFromCardNumber:updatedText];

    NSInteger sectionIdentifier = [self.collectionViewModel
        sectionIdentifierForSection:[indexPath section]];
    // Update the cell.
    [self reconfigureCellsForItems:@[ item ]
           inSectionWithIdentifier:sectionIdentifier];
  }

  return YES;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  switch (item.type) {
    case ItemTypeBillingAddress: {
      CollectionViewDetailCell* billingCell =
          base::mac::ObjCCastStrict<CollectionViewDetailCell>(cell);
      billingCell.textLabel.font = [MDCTypography body2Font];
      billingCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
      billingCell.detailTextLabel.font = [MDCTypography body1Font];
      billingCell.detailTextLabel.textColor =
          [[MDCPalette cr_bluePalette] tint600];
      break;
    }
    case ItemTypeSaveCard: {
      CollectionViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<CollectionViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(saveCardSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeAcceptedMethods:
    case ItemTypeCardSummary:
    case ItemTypeSaveCard:
      break;
    case ItemTypeBillingAddress: {
      // TODO(crbug.com/602666): Display a list of billing addresses.
      break;
    }
    default:
      break;
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeBillingAddress:
      return MDCCellDefaultOneLineHeight;
    case ItemTypeAcceptedMethods:
    case ItemTypeCardSummary:
    case ItemTypeSaveCard:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      return
          [super collectionView:collectionView cellHeightAtIndexPath:indexPath];
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeAcceptedMethods:
    case ItemTypeCardSummary:
    case ItemTypeSaveCard:
      return YES;
    default:
      return [super collectionView:collectionView
           hidesInkViewAtIndexPath:indexPath];
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeAcceptedMethods:
      return YES;
    default:
      return [super collectionView:collectionView
          shouldHideItemBackgroundAtIndexPath:indexPath];
  }
}

#pragma mark Switch Actions

- (void)saveCardSwitchToggled:(UISwitch*)sender {
  _saveCreditCard = sender.isOn;
}

@end
