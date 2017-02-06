// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
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

NSString* const kPaymentMethodSelectionCollectionViewId =
    @"kPaymentMethodSelectionCollectionViewId";

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

@interface PaymentMethodSelectionViewController () {
  base::WeakNSProtocol<id<PaymentMethodSelectionViewControllerDelegate>>
      _delegate;

  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;
}

// Called when the user presses the return button.
- (void)onReturn;

@end

@implementation PaymentMethodSelectionViewController

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(
                       IDS_IOS_PAYMENT_REQUEST_METHOD_SELECTION_TITLE)];

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

- (id<PaymentMethodSelectionViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentMethodSelectionViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)onReturn {
  [_delegate paymentMethodSelectionViewControllerDidReturn:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierPayment];

  for (const auto& paymentMethod : _paymentRequest->credit_cards()) {
    PaymentMethodItem* paymentMethodItem = [[[PaymentMethodItem alloc]
        initWithType:ItemTypePaymentMethod] autorelease];
    paymentMethodItem.accessibilityTraits |= UIAccessibilityTraitButton;
    paymentMethodItem.methodID =
        base::SysUTF16ToNSString(paymentMethod->TypeAndLastFourDigits());
    paymentMethodItem.methodDetail = base::SysUTF16ToNSString(
        paymentMethod->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

    int methodTypeIconID =
        autofill::data_util::GetPaymentRequestData(paymentMethod->type())
            .icon_resource_id;
    paymentMethodItem.methodTypeIcon = NativeImage(methodTypeIconID);

    if (_paymentRequest->selected_credit_card() == paymentMethod)
      paymentMethodItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [model addItem:paymentMethodItem
        toSectionWithIdentifier:SectionIdentifierPayment];
  }

  PaymentsTextItem* addPaymentMethod =
      [[[PaymentsTextItem alloc] initWithType:ItemTypeAddMethod] autorelease];
  addPaymentMethod.text =
      l10n_util::GetNSString(IDS_IOS_PAYMENT_REQUEST_ADD_METHOD_BUTTON);
  addPaymentMethod.image = NativeImage(IDR_IOS_PAYMENTS_ADD);
  addPaymentMethod.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:addPaymentMethod
      toSectionWithIdentifier:SectionIdentifierPayment];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentMethodSelectionCollectionViewId;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypePaymentMethod) {
    DCHECK(indexPath.item < (NSInteger)_paymentRequest->credit_cards().size());
    [_delegate
        paymentMethodSelectionViewController:self
                      didSelectPaymentMethod:_paymentRequest->credit_cards()
                                                 [indexPath.item]];
  }
  // TODO(crbug.com/602666): Present a credit card addition UI when
  //     itemType == ItemAddMethod.
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
