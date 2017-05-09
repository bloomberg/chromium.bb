// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/payments/cells/payments_has_accessory_type.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller_actions.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller_data_source.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kPaymentRequestSelectorCollectionViewAccessibilityID =
    @"kPaymentRequestSelectorCollectionViewAccessibilityID";

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierItems = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSelectableItem,  // This is a repeated item type.
  ItemTypeSpinner,
  ItemTypeAddItem,
};

}  // namespace

@interface PaymentRequestSelectorViewController ()<
    PaymentRequestSelectorViewControllerActions>

@end

@implementation PaymentRequestSelectorViewController

@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;

- (instancetype)init {
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    // Set up leading (back) button.
    UIBarButtonItem* backButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onBack)];
    backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    self.navigationItem.leftBarButtonItem = backButton;
  }
  return self;
}

#pragma mark - PaymentRequestSelectorViewControllerActions

- (void)onBack {
  [self.delegate paymentRequestSelectorViewControllerDidFinish:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierItems];

  // If the view controller is in the pending state, only display a spinner and
  // a message indicating the pending state.
  if (self.dataSource.state == PaymentRequestSelectorStatePending) {
    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.state = StatusItemState::VERIFYING;
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_CHECKING_OPTION);
    [model addItem:statusItem toSectionWithIdentifier:SectionIdentifierItems];
    return;
  }

  CollectionViewItem* headerItem = [self.dataSource headerItem];
  if (headerItem) {
    headerItem.type = ItemTypeHeader;
    [model addItem:headerItem toSectionWithIdentifier:SectionIdentifierItems];
  }

  [self.dataSource.selectableItems enumerateObjectsUsingBlock:^(
                                       CollectionViewItem* item,
                                       NSUInteger index, BOOL* stop) {
    DCHECK([item conformsToProtocol:@protocol(PaymentsHasAccessoryType)]);
    CollectionViewItem<PaymentsHasAccessoryType>* selectableItem =
        reinterpret_cast<CollectionViewItem<PaymentsHasAccessoryType>*>(item);
    selectableItem.type = ItemTypeSelectableItem;
    selectableItem.accessibilityTraits |= UIAccessibilityTraitButton;
    selectableItem.accessoryType = (index == self.dataSource.selectedItemIndex)
                                       ? MDCCollectionViewCellAccessoryCheckmark
                                       : MDCCollectionViewCellAccessoryNone;
    [model addItem:selectableItem
        toSectionWithIdentifier:SectionIdentifierItems];
  }];

  CollectionViewItem* addButtonItem = [self.dataSource addButtonItem];
  if (addButtonItem) {
    addButtonItem.type = ItemTypeAddItem;
    addButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
    [model addItem:addButtonItem
        toSectionWithIdentifier:SectionIdentifierItems];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentRequestSelectorCollectionViewAccessibilityID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  CollectionViewModel* model = self.collectionViewModel;

  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeHeader: {
      if ([cell isKindOfClass:[PaymentsTextCell class]]) {
        PaymentsTextCell* textCell =
            base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
        textCell.textLabel.font = [MDCTypography body2Font];
        textCell.textLabel.textColor =
            self.dataSource.state == PaymentRequestSelectorStateError
                ? [[MDCPalette cr_redPalette] tint600]
                : [[MDCPalette greyPalette] tint600];
      }
      break;
    }
    case ItemTypeSelectableItem: {
      if ([cell isKindOfClass:[PaymentsTextCell class]]) {
        PaymentsTextCell* textCell =
            base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
        textCell.textLabel.font = [MDCTypography body2Font];
        textCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
        textCell.detailTextLabel.font = [MDCTypography body1Font];
        textCell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
      }
      break;
    }
    case ItemTypeAddItem:
      if ([cell isKindOfClass:[PaymentsTextCell class]]) {
        PaymentsTextCell* textCell =
            base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
        textCell.textLabel.font = [MDCTypography body2Font];
        textCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
      }
      break;
    default:
      break;
  }

  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem* item = [model itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeSelectableItem: {
      // Update the currently selected cell, if any.
      if (self.dataSource.selectedItemIndex != NSUIntegerMax) {
        CollectionViewItem<PaymentsHasAccessoryType>* oldSelectedItem =
            reinterpret_cast<CollectionViewItem<PaymentsHasAccessoryType>*>(
                [self.dataSource
                    selectableItemAtIndex:self.dataSource.selectedItemIndex]);
        oldSelectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        [self reconfigureCellsForItems:@[ oldSelectedItem ]];
      }

      // Update the newly selected cell.
      CollectionViewItem<PaymentsHasAccessoryType>* newSelectedItem =
          reinterpret_cast<CollectionViewItem<PaymentsHasAccessoryType>*>(item);
      newSelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      [self reconfigureCellsForItems:@[ newSelectedItem ]];

      // Notify the delegate of the selection.
      NSUInteger index =
          [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
      DCHECK(index < [[self.dataSource selectableItems] count]);
      [self.delegate paymentRequestSelectorViewController:self
                                     didSelectItemAtIndex:index];
      break;
    }
    case ItemTypeAddItem: {
      if ([self.delegate
              respondsToSelector:@selector
              (paymentRequestSelectorViewControllerDidSelectAddItem:)]) {
        [self.delegate
            paymentRequestSelectorViewControllerDidSelectAddItem:self];
      }
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

  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeHeader) {
    return YES;
  } else {
    return NO;
  }
}

@end
