// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_selector_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/payment_request_selector_view_controller_actions.h"
#import "ios/chrome/browser/payments/payment_request_selector_view_controller_data_source.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
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

  [self.dataSource.selectableItems
      enumerateObjectsUsingBlock:^(
          CollectionViewItem<PaymentsHasAccessoryType>* item, NSUInteger index,
          BOOL* stop) {
        item.type = ItemTypeSelectableItem;
        item.accessibilityTraits |= UIAccessibilityTraitButton;
        item.accessoryType = (index == self.dataSource.selectedItemIndex)
                                 ? MDCCollectionViewCellAccessoryCheckmark
                                 : MDCCollectionViewCellAccessoryNone;
        [model addItem:item toSectionWithIdentifier:SectionIdentifierItems];
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

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem<PaymentsHasAccessoryType>* item =
      [model itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeSelectableItem: {
      // Update the currently selected cell, if any.
      if (self.dataSource.selectedItemIndex != NSUIntegerMax) {
        CollectionViewItem<PaymentsHasAccessoryType>* selectedItem =
            [self.dataSource
                selectableItemAtIndex:self.dataSource.selectedItemIndex];
        selectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        [self reconfigureCellsForItems:@[ selectedItem ]
               inSectionWithIdentifier:SectionIdentifierItems];
      }

      // Update the newly selected cell.
      item.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      [self reconfigureCellsForItems:@[ item ]
             inSectionWithIdentifier:SectionIdentifierItems];

      // Notify the delegate of the selection.
      NSUInteger index =
          [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
      DCHECK(index < [[self.dataSource selectableItems] count]);
      [self.delegate paymentRequestSelectorViewController:self
                                     didSelectItemAtIndex:index];
      break;
    }
    case ItemTypeAddItem: {
      [self.delegate paymentRequestSelectorViewControllerDidSelectAddItem:self];
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
