// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"

#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/payments/shipping_address_selection_view_controller_actions.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
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
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressSelectorTitle;
using ::payment_request_util::GetShippingAddressSelectorInfoMessage;

NSString* const kShippingAddressSelectionCollectionViewID =
    @"kShippingAddressSelectionCollectionViewID";

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

@interface ShippingAddressSelectionViewController ()<
    ShippingAddressSelectionViewControllerActions> {
  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;

  // The currently selected item. May be nil.
  __weak AutofillProfileItem* _selectedItem;
}

@end

@implementation ShippingAddressSelectionViewController

@synthesize pending = _pending;
@synthesize errorMessage = _errorMessage;
@synthesize delegate = _delegate;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    self.title = GetShippingAddressSelectorTitle(*paymentRequest);

    // Set up leading (return) button.
    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    returnButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    self.navigationItem.leftBarButtonItem = returnButton;

    _paymentRequest = paymentRequest;
  }
  return self;
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

  if (_pending) {
    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_CHECKING_OPTION);
    [model addItem:statusItem
        toSectionWithIdentifier:SectionIdentifierShippingAddress];
    return;
  }

  PaymentsTextItem* messageItem =
      [[PaymentsTextItem alloc] initWithType:ItemTypeMessage];
  if (_errorMessage) {
    messageItem.text = _errorMessage;
    messageItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
  } else {
    messageItem.text = GetShippingAddressSelectorInfoMessage(*_paymentRequest);
  }
  [model addItem:messageItem
      toSectionWithIdentifier:SectionIdentifierShippingAddress];

  for (auto* shippingAddress : _paymentRequest->shipping_profiles()) {
    DCHECK(shippingAddress);
    AutofillProfileItem* item =
        [[AutofillProfileItem alloc] initWithType:ItemTypeShippingAddress];
    item.accessibilityTraits |= UIAccessibilityTraitButton;
    item.name = GetNameLabelFromAutofillProfile(*shippingAddress);
    item.address = GetAddressLabelFromAutofillProfile(*shippingAddress);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*shippingAddress);
    if (_paymentRequest->selected_shipping_profile() == shippingAddress) {
      item.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      _selectedItem = item;
    }
    [model addItem:item
        toSectionWithIdentifier:SectionIdentifierShippingAddress];
  }

  PaymentsTextItem* addShippingAddress =
      [[PaymentsTextItem alloc] initWithType:ItemTypeAddShippingAddress];
  addShippingAddress.text = l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS);
  addShippingAddress.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  addShippingAddress.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:addShippingAddress
      toSectionWithIdentifier:SectionIdentifierShippingAddress];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kShippingAddressSelectionCollectionViewID;

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
    AutofillProfileItem* newlySelectedItem =
        base::mac::ObjCCastStrict<AutofillProfileItem>(item);
    newlySelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [self reconfigureCellsForItems:@[ newlySelectedItem ]
           inSectionWithIdentifier:SectionIdentifierShippingAddress];

    // Update the reference to the selected item.
    _selectedItem = newlySelectedItem;

    // Notify the delegate of the selection.
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    DCHECK(index < (NSInteger)_paymentRequest->shipping_profiles().size());
    [_delegate shippingAddressSelectionViewController:self
                             didSelectShippingAddress:
                                 _paymentRequest->shipping_profiles()[index]];
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
