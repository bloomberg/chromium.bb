// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/payment_request_utils.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

NSString* const kShippingOptionSelectionCollectionViewId =
    @"kShippingOptionSelectionCollectionViewId";

namespace {

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierShippingOption = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSpinner = kItemTypeEnumZero,
  ItemTypeMessage,
  ItemTypeShippingOption,  // This is a repeated item type.
};

}  // namespace

@interface ShippingOptionSelectionViewController () {
  base::WeakNSProtocol<id<ShippingOptionSelectionViewControllerDelegate>>
      _delegate;

  CollectionViewTextItem* _selectedItem;
}

// Called when the user presses the return button.
- (void)onReturn;

@end

@implementation ShippingOptionSelectionViewController

@synthesize shippingOptions = _shippingOptions;
@synthesize selectedShippingOption = _selectedShippingOption;
@synthesize isLoading = _isLoading;
@synthesize errorMessage = _errorMessage;

- (instancetype)init {
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_PAYMENT_REQUEST_SHIPPING_OPTION_SELECTION_TITLE);

    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    returnButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    self.navigationItem.leftBarButtonItem = returnButton;
  }
  return self;
}

- (id<ShippingOptionSelectionViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:
    (id<ShippingOptionSelectionViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)onReturn {
  [_delegate shippingOptionSelectionViewControllerDidReturn:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  _selectedItem = nil;

  [model addSectionWithIdentifier:SectionIdentifierShippingOption];

  if (self.isLoading) {
    StatusItem* statusItem =
        [[[StatusItem alloc] initWithType:ItemTypeSpinner] autorelease];
    statusItem.text =
        l10n_util::GetNSString(IDS_IOS_PAYMENT_REQUEST_CHECKING_LABEL);
    [model addItem:statusItem
        toSectionWithIdentifier:SectionIdentifierShippingOption];
    return;
  }

  if (_errorMessage) {
    PaymentsTextItem* messageItem =
        [[[PaymentsTextItem alloc] initWithType:ItemTypeMessage] autorelease];
    messageItem.text = _errorMessage;
    messageItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
    [model addItem:messageItem
        toSectionWithIdentifier:SectionIdentifierShippingOption];
  }

  for (size_t i = 0; i < _shippingOptions.size(); ++i) {
    web::PaymentShippingOption* shippingOption = _shippingOptions[i];
    CollectionViewTextItem* item = [[[CollectionViewTextItem alloc]
        initWithType:ItemTypeShippingOption] autorelease];
    item.text = base::SysUTF16ToNSString(shippingOption->label);
    NSString* currencyCode =
        base::SysUTF16ToNSString(shippingOption->amount.currency);
    NSDecimalNumber* value = [NSDecimalNumber
        decimalNumberWithString:SysUTF16ToNSString(
                                    shippingOption->amount.value)];
    item.detailText =
        payment_request_utils::FormattedCurrencyString(value, currencyCode);

    // Styling.
    item.textFont = [MDCTypography body2Font];
    item.textColor = [[MDCPalette greyPalette] tint900];
    item.detailTextFont = [MDCTypography body1Font];
    item.detailTextColor = [[MDCPalette greyPalette] tint900];

    if (_selectedShippingOption == shippingOption) {
      item.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      _selectedItem = item;
    }

    [model addItem:item
        toSectionWithIdentifier:SectionIdentifierShippingOption];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kShippingOptionSelectionCollectionViewId;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeMessage: {
      PaymentsTextCell* messageCell =
          base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
      messageCell.textLabel.textColor = [[MDCPalette cr_redPalette] tint600];
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

  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem* item = [model itemAtIndexPath:indexPath];
  if (item.type == ItemTypeShippingOption) {
    // Update the currently selected cell, if any.
    if (_selectedItem) {
      _selectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
      [self reconfigureCellsForItems:@[ _selectedItem ]
             inSectionWithIdentifier:SectionIdentifierShippingOption];
    }

    // Update the newly selected cell.
    CollectionViewTextItem* newlySelectedItem =
        base::mac::ObjCCastStrict<CollectionViewTextItem>(item);
    newlySelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [self reconfigureCellsForItems:@[ newlySelectedItem ]
           inSectionWithIdentifier:SectionIdentifierShippingOption];

    // Update the selected shipping option and its respective item.
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    DCHECK(index < (NSInteger)_shippingOptions.size());
    self.selectedShippingOption = _shippingOptions[index];
    _selectedItem = newlySelectedItem;

    // Notify the delegate of the selection.
    [_delegate
        shippingOptionSelectionViewController:self
                       selectedShippingOption:self.selectedShippingOption];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeMessage:
    case ItemTypeSpinner:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeShippingOption:
      return MDCCellDefaultTwoLineHeight;
    default:
      NOTREACHED();
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeMessage) {
    return YES;
  } else {
    return NO;
  }
}

@end
