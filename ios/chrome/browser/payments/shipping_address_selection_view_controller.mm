// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/cells/shipping_address_item.h"
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
#include "ui/base/l10n/l10n_util.h"

using payment_request_utils::NameLabelFromAutofillProfile;
using payment_request_utils::AddressLabelFromAutofillProfile;
using payment_request_utils::PhoneNumberLabelFromAutofillProfile;

NSString* const kShippingAddressSelectionCollectionViewId =
    @"kShippingAddressSelectionCollectionViewId";

namespace {

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierShippingAddress = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSpinner = kItemTypeEnumZero,
  ItemTypeMessage,
  ItemTypeShippingAddress,  // This is a repeated item type.
  ItemTypeAddShippingAddress,
};

}  // namespace

@interface ShippingAddressSelectionViewController () {
  base::WeakNSProtocol<id<ShippingAddressSelectionViewControllerDelegate>>
      _delegate;

  ShippingAddressItem* _selectedItem;
}

// Called when the user presses the return button.
- (void)onReturn;

@end

@implementation ShippingAddressSelectionViewController

@synthesize selectedShippingAddress = _selectedShippingAddress;
@synthesize shippingAddresses = _shippingAddresses;
@synthesize isLoading = _isLoading;
@synthesize errorMessage = _errorMessage;

- (instancetype)init {
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_TITLE);

    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    returnButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    self.navigationItem.leftBarButtonItem = returnButton;
  }
  return self;
}

- (id<ShippingAddressSelectionViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:
    (id<ShippingAddressSelectionViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)onReturn {
  [_delegate shippingAddressSelectionViewControllerDidReturn:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  _selectedItem = nil;

  [model addSectionWithIdentifier:SectionIdentifierShippingAddress];

  if (_isLoading) {
    StatusItem* statusItem =
        [[[StatusItem alloc] initWithType:ItemTypeSpinner] autorelease];
    statusItem.text =
        l10n_util::GetNSString(IDS_IOS_PAYMENT_REQUEST_CHECKING_LABEL);
    [model addItem:statusItem
        toSectionWithIdentifier:SectionIdentifierShippingAddress];
    return;
  }

  PaymentsTextItem* messageItem =
      [[[PaymentsTextItem alloc] initWithType:ItemTypeMessage] autorelease];
  if (_errorMessage) {
    messageItem.text = _errorMessage;
    messageItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
  } else {
    messageItem.text = l10n_util::GetNSString(
        IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_MESSAGE);
  }
  [model addItem:messageItem
      toSectionWithIdentifier:SectionIdentifierShippingAddress];

  for (size_t i = 0; i < _shippingAddresses.size(); ++i) {
    autofill::AutofillProfile* shippingAddress = _shippingAddresses[i];
    ShippingAddressItem* item = [[[ShippingAddressItem alloc]
        initWithType:ItemTypeShippingAddress] autorelease];
    item.accessibilityTraits |= UIAccessibilityTraitButton;
    item.name = NameLabelFromAutofillProfile(shippingAddress);
    item.address = AddressLabelFromAutofillProfile(shippingAddress);
    item.phoneNumber = PhoneNumberLabelFromAutofillProfile(shippingAddress);
    if (shippingAddress == _selectedShippingAddress) {
      item.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      _selectedItem = item;
    }
    [model addItem:item
        toSectionWithIdentifier:SectionIdentifierShippingAddress];
  }

  PaymentsTextItem* addShippingAddress = [[[PaymentsTextItem alloc]
      initWithType:ItemTypeAddShippingAddress] autorelease];
  addShippingAddress.text = l10n_util::GetNSString(
      IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_ADD_BUTTON);
  addShippingAddress.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  addShippingAddress.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:addShippingAddress
      toSectionWithIdentifier:SectionIdentifierShippingAddress];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kShippingAddressSelectionCollectionViewId;

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
      messageCell.textLabel.textColor =
          _errorMessage ? [[MDCPalette cr_redPalette] tint600]
                        : [[MDCPalette greyPalette] tint600];
      break;
    }
    case ItemTypeAddShippingAddress: {
      PaymentsTextCell* addAddressCell =
          base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
      addAddressCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
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
  if (item.type == ItemTypeShippingAddress) {
    // Update the currently selected cell, if any.
    if (_selectedItem) {
      _selectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
      [self reconfigureCellsForItems:@[ _selectedItem ]
             inSectionWithIdentifier:SectionIdentifierShippingAddress];
    }

    // Update the newly selected cell.
    ShippingAddressItem* newlySelectedItem =
        base::mac::ObjCCastStrict<ShippingAddressItem>(item);
    newlySelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [self reconfigureCellsForItems:@[ newlySelectedItem ]
           inSectionWithIdentifier:SectionIdentifierShippingAddress];

    // Update the selected shipping address and its respective item.
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    DCHECK(index < (NSInteger)_shippingAddresses.size());
    self.selectedShippingAddress = _shippingAddresses[index];
    _selectedItem = newlySelectedItem;

    // Notify the delegate of the selection.
    [_delegate
        shippingAddressSelectionViewController:self
                       selectedShippingAddress:self.selectedShippingAddress];
  }
  // TODO(crbug.com/602666): Present a shipping address addition UI when
  // itemType == ItemTypeAddShippingAddress.
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeSpinner:
    case ItemTypeMessage:
    case ItemTypeShippingAddress:
    case ItemTypeAddShippingAddress:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
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
