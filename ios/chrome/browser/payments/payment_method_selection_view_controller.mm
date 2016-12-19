// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/grit/ios_strings.h"
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
}

// Called when the user presses the return button.
- (void)onReturn;

@end

@implementation PaymentMethodSelectionViewController

@synthesize selectedPaymentMethod = _selectedPaymentMethod;
@synthesize paymentMethods = _paymentMethods;

- (instancetype)init {
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(
                       IDS_IOS_PAYMENT_REQUEST_METHOD_SELECTION_TITLE)];

    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    [self navigationItem].leftBarButtonItem = returnButton;
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

  for (size_t i = 0; i < _paymentMethods.size(); ++i) {
    autofill::CreditCard* paymentMethod = _paymentMethods[i];
    PaymentMethodItem* paymentMethodItem = [[[PaymentMethodItem alloc]
        initWithType:ItemTypePaymentMethod] autorelease];
    paymentMethodItem.methodID =
        base::SysUTF16ToNSString(paymentMethod->TypeAndLastFourDigits());
    paymentMethodItem.methodDetail = base::SysUTF16ToNSString(
        paymentMethod->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

    int methodTypeIconID =
        autofill::data_util::GetPaymentRequestData(paymentMethod->type())
            .icon_resource_id;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    paymentMethodItem.methodTypeIcon =
        rb.GetNativeImageNamed(methodTypeIconID).ToUIImage();

    if (paymentMethod == _selectedPaymentMethod)
      paymentMethodItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
    [model addItem:paymentMethodItem
        toSectionWithIdentifier:SectionIdentifierPayment];
  }

  CollectionViewTextItem* addPaymentMethod = [[[CollectionViewTextItem alloc]
      initWithType:ItemTypeAddMethod] autorelease];
  addPaymentMethod.text =
      l10n_util::GetNSString(IDS_IOS_PAYMENT_REQUEST_ADD_METHOD_BUTTON);
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
    DCHECK(indexPath.item < (NSInteger)_paymentMethods.size());
    [_delegate
        paymentMethodSelectionViewController:self
                       selectedPaymentMethod:_paymentMethods[indexPath.item]];
  }
  // TODO(crbug.com/602666): Present a credit card addition UI when
  //     itemType == ItemAddMethod.
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeAddMethod)
    return MDCCellDefaultOneLineHeight;
  else
    return MDCCellDefaultTwoLineHeight;
}

@end
