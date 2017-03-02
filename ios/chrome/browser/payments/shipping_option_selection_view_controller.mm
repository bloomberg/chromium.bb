// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/payments/shipping_option_selection_view_controller_actions.h"
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
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetShippingOptionSelectorTitle;

NSString* const kShippingOptionSelectionCollectionViewID =
    @"kShippingOptionSelectionCollectionViewID";

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

@interface ShippingOptionSelectionViewController ()<
    ShippingOptionSelectionViewControllerActions> {
  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;

  // The currently selected item. May be nil.
  CollectionViewTextItem* _selectedItem;
}

@end

@implementation ShippingOptionSelectionViewController

@synthesize pending = _pending;
@synthesize errorMessage = _errorMessage;
@synthesize delegate = _delegate;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    self.title = GetShippingOptionSelectorTitle(*paymentRequest);

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
  [_delegate shippingOptionSelectionViewControllerDidReturn:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  _selectedItem = nil;

  [model addSectionWithIdentifier:SectionIdentifierShippingOption];

  if (self.pending) {
    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_CHECKING_OPTION);
    [model addItem:statusItem
        toSectionWithIdentifier:SectionIdentifierShippingOption];
    return;
  }

  if (_errorMessage) {
    PaymentsTextItem* messageItem =
        [[PaymentsTextItem alloc] initWithType:ItemTypeMessage];
    messageItem.text = _errorMessage;
    messageItem.image = NativeImage(IDR_IOS_PAYMENTS_WARNING);
    [model addItem:messageItem
        toSectionWithIdentifier:SectionIdentifierShippingOption];
  }

  for (const auto* shippingOption : _paymentRequest->shipping_options()) {
    CollectionViewTextItem* item =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeShippingOption];
    item.text = base::SysUTF16ToNSString(shippingOption->label);
    payments::CurrencyFormatter* currencyFormatter =
        _paymentRequest->GetOrCreateCurrencyFormatter();
    item.detailText = SysUTF16ToNSString(currencyFormatter->Format(
        base::UTF16ToASCII(shippingOption->amount.value)));

    // Styling.
    item.textFont = [MDCTypography body2Font];
    item.textColor = [[MDCPalette greyPalette] tint900];
    item.detailTextFont = [MDCTypography body1Font];
    item.detailTextColor = [[MDCPalette greyPalette] tint900];

    if (_paymentRequest->selected_shipping_option() == shippingOption) {
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
      kShippingOptionSelectionCollectionViewID;

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

    // Update the reference to the selected item.
    _selectedItem = newlySelectedItem;

    // Notify the delegate of the selection.
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    DCHECK(index < (NSInteger)_paymentRequest->shipping_options().size());
    [_delegate
        shippingOptionSelectionViewController:self
                      didSelectShippingOption:_paymentRequest
                                                  ->shipping_options()[index]];
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
