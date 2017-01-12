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
#import "ios/chrome/browser/payments/cells/shipping_address_item.h"
#import "ios/chrome/browser/payments/payment_request_utils.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/grit/ios_strings.h"
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
  ItemTypeMessage = kItemTypeEnumZero,
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

  [model addSectionWithIdentifier:SectionIdentifierShippingAddress];

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

  CollectionViewTextItem* addShippingAddress = [[[CollectionViewTextItem alloc]
      initWithType:ItemTypeAddShippingAddress] autorelease];
  addShippingAddress.text = l10n_util::GetNSString(
      IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_ADD_BUTTON);
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

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewModel* model = self.collectionViewModel;

  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeShippingAddress) {
    NSIndexPath* currentlySelectedIndexPath = [self.collectionViewModel
               indexPathForItem:_selectedItem
        inSectionWithIdentifier:SectionIdentifierShippingAddress];
    if (currentlySelectedIndexPath != indexPath) {
      // Update the cells.
      CollectionViewItem* item = [model itemAtIndexPath:indexPath];
      ShippingAddressItem* newlySelectedItem =
          base::mac::ObjCCastStrict<ShippingAddressItem>(item);
      newlySelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;

      _selectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;

      [self reconfigureCellsForItems:@[ _selectedItem, newlySelectedItem ]
             inSectionWithIdentifier:SectionIdentifierShippingAddress];

      // Update the selected shipping address and its respective item.
      NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
      DCHECK(index < (NSInteger)_shippingAddresses.size());
      self.selectedShippingAddress = _shippingAddresses[index];
      _selectedItem = newlySelectedItem;
    }
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
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeAddShippingAddress) {
    return MDCCellDefaultOneLineHeight;
  } else {
    CollectionViewItem* item =
        [self.collectionViewModel itemAtIndexPath:indexPath];
    return [MDCCollectionViewCell
        cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                           forItem:item];
  }
}

@end
