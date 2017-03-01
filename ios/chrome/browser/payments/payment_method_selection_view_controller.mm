// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/payment_method_selection_view_controller_actions.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPaymentMethodSelectionCollectionViewID =
    @"kPaymentMethodSelectionCollectionViewID";

namespace {

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPayment = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePaymentMethod = kItemTypeEnumZero,  // This is a repeated item type.
  ItemTypeAddMethod,
};

}  // namespace

@interface PaymentMethodSelectionViewController ()<
    PaymentMethodSelectionViewControllerActions> {
  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;

  // The currently selected item. May be nil.
  __weak PaymentMethodItem* _selectedItem;
}

@end

@implementation PaymentMethodSelectionViewController
@synthesize delegate = _delegate;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    [self
        setTitle:l10n_util::GetNSString(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL)];

    // Set up leading (return) button.
    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    returnButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    [self navigationItem].leftBarButtonItem = returnButton;

    _paymentRequest = paymentRequest;
  }
  return self;
}

- (void)onReturn {
  [_delegate paymentMethodSelectionViewControllerDidReturn:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  _selectedItem = nil;

  [model addSectionWithIdentifier:SectionIdentifierPayment];

  for (const auto* paymentMethod : _paymentRequest->credit_cards()) {
    PaymentMethodItem* paymentMethodItem =
        [[PaymentMethodItem alloc] initWithType:ItemTypePaymentMethod];
    paymentMethodItem.accessibilityTraits |= UIAccessibilityTraitButton;
    paymentMethodItem.methodID =
        base::SysUTF16ToNSString(paymentMethod->TypeAndLastFourDigits());
    paymentMethodItem.methodDetail = base::SysUTF16ToNSString(
        paymentMethod->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
    int methodTypeIconID =
        autofill::data_util::GetPaymentRequestData(paymentMethod->type())
            .icon_resource_id;
    paymentMethodItem.methodTypeIcon = NativeImage(methodTypeIconID);

    if (_paymentRequest->selected_credit_card() == paymentMethod) {
      paymentMethodItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      _selectedItem = paymentMethodItem;
    }
    [model addItem:paymentMethodItem
        toSectionWithIdentifier:SectionIdentifierPayment];
  }

  PaymentsTextItem* addPaymentMethod =
      [[PaymentsTextItem alloc] initWithType:ItemTypeAddMethod];
  addPaymentMethod.text = l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD);
  addPaymentMethod.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  addPaymentMethod.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:addPaymentMethod
      toSectionWithIdentifier:SectionIdentifierPayment];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentMethodSelectionCollectionViewID;

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

  CollectionViewItem* item = [model itemAtIndexPath:indexPath];
  if (item.type == ItemTypePaymentMethod) {
    // Update the currently selected cell, if any.
    if (_selectedItem) {
      _selectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
      [self reconfigureCellsForItems:@[ _selectedItem ]
             inSectionWithIdentifier:SectionIdentifierPayment];
    }

    // Update the newly selected cell.
    PaymentMethodItem* newlySelectedItem =
        base::mac::ObjCCastStrict<PaymentMethodItem>(item);
    newlySelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [self reconfigureCellsForItems:@[ newlySelectedItem ]
           inSectionWithIdentifier:SectionIdentifierPayment];

    // Update the reference to the selected item.
    _selectedItem = newlySelectedItem;

    // Notify the delegate of the selection.
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    DCHECK(index < (NSInteger)_paymentRequest->credit_cards().size());
    [_delegate
        paymentMethodSelectionViewController:self
                      didSelectPaymentMethod:_paymentRequest
                                                 ->credit_cards()[index]];
  }
  // TODO(crbug.com/602666): Present a credit card addition UI when
  // itemType == ItemTypeAddMethod.
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypePaymentMethod:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeAddMethod:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      NOTREACHED();
      return MDCCellDefaultOneLineHeight;
  }
}

@end
