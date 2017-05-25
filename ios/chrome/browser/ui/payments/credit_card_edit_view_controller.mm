// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller+internal.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kCreditCardEditCollectionViewId =
    @"kCreditCardEditCollectionViewId";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveCard = kSectionIdentifierEnumStart,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSaveCard = kItemTypeEnumStart,
};

}  // namespace

@interface CreditCardEditViewController () {
  // Indicates whether the credit card being created should be saved locally.
  BOOL _saveCreditCard;
}

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* fields;

@end

@implementation CreditCardEditViewController

@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;
@synthesize fields = _fields;

#pragma mark - Setters

- (void)setDelegate:(id<CreditCardEditViewControllerDelegate>)delegate {
  [super setDelegate:delegate];
  _delegate = delegate;
}

- (void)setDataSource:(id<CreditCardEditViewControllerDataSource>)dataSource {
  [super setDataSource:dataSource];
  _dataSource = dataSource;
}

#pragma mark - PaymentRequestEditViewControllerActions methods

- (void)onCancel {
  [super onCancel];

  [_delegate creditCardEditViewControllerDidCancel:self];
}

- (void)onDone {
  [super onDone];

  if (![self validateForm])
    return;

  [_delegate creditCardEditViewController:self
                   didFinishEditingFields:_fields
                           saveCreditCard:_saveCreditCard];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];

  // If editing a credit card, set the card type icon (e.g. "Visa").
  if (_dataSource.state == EditViewControllerStateEdit) {
    for (EditorField* field in _fields) {
      if (field.autofillUIType == AutofillUITypeCreditCardNumber) {
        AutofillEditItem* item =
            base::mac::ObjCCastStrict<AutofillEditItem>(field.item);
        item.cardTypeIcon =
            [_dataSource cardTypeIconFromCardNumber:item.textFieldValue];
      }
    }
  }
}

- (void)loadFooterItems {
  CollectionViewModel* model = self.collectionViewModel;

  // "Save card" section. Visible only when creating a card.
  if (_dataSource.state == EditViewControllerStateCreate) {
    [model addSectionWithIdentifier:SectionIdentifierSaveCard];
    CollectionViewSwitchItem* saveCardItem =
        [[CollectionViewSwitchItem alloc] initWithType:ItemTypeSaveCard];
    saveCardItem.text =
        l10n_util::GetNSString(IDS_PAYMENTS_SAVE_CARD_TO_DEVICE_CHECKBOX);
    saveCardItem.on = YES;
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

    // Update the cell.
    [self reconfigureCellsForItems:@[ item ]];
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

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
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
    case ItemTypeSaveCard:
      return YES;
    default:
      return [super collectionView:collectionView
           hidesInkViewAtIndexPath:indexPath];
  }
}

#pragma mark Switch Actions

- (void)saveCardSwitchToggled:(UISwitch*)sender {
  _saveCreditCard = sender.isOn;
}

@end
