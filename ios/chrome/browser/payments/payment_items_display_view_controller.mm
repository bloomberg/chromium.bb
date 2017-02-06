// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/price_item.h"
#import "ios/chrome/browser/payments/payment_request_utils.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util.h"

NSString* const kPaymentItemsDisplayCollectionViewId =
    @"kPaymentItemsDisplayCollectionViewId";
NSString* const kPaymentItemsDisplayItemId = @"kPaymentItemsDisplayItemId";

namespace {

const CGFloat kButtonEdgeInset = 9;
const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPayment = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePaymentItemTotal = kItemTypeEnumZero,
  ItemTypePaymentItem,  // This is a repeated item type.
};

}  // namespace

@interface PaymentItemsDisplayViewController () {
  base::WeakNSProtocol<id<PaymentItemsDisplayViewControllerDelegate>> _delegate;
  base::scoped_nsobject<MDCFlatButton> _payButton;

  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;
}

// Called when the user presses the return button.
- (void)onReturn;

// Called when the user presses the pay button.
- (void)onConfirm;

@end

@implementation PaymentItemsDisplayViewController

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                      payButtonEnabled:(BOOL)payButtonEnabled {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(
                       IDS_IOS_PAYMENT_REQUEST_PAYMENT_ITEMS_TITLE)];

    // Set up left (return) button.
    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:nil
                                            action:@selector(onReturn)];
    [returnButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_BACK)];
    [self navigationItem].leftBarButtonItem = returnButton;

    // Set up right (pay) button.
    _payButton.reset([[MDCFlatButton alloc] init]);
    [_payButton
        setTitle:l10n_util::GetNSString(IDS_IOS_PAYMENT_REQUEST_PAY_BUTTON)
        forState:UIControlStateNormal];
    [_payButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                          forState:UIControlStateNormal];
    [_payButton setInkColor:[UIColor colorWithWhite:1 alpha:0.2]];
    [_payButton setBackgroundColor:[UIColor grayColor]
                          forState:UIControlStateDisabled];
    [_payButton addTarget:nil
                   action:@selector(onConfirm)
         forControlEvents:UIControlEventTouchUpInside];
    [_payButton sizeToFit];
    [_payButton setEnabled:payButtonEnabled];
    [_payButton setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                                    UIViewAutoresizingFlexibleTopMargin |
                                    UIViewAutoresizingFlexibleBottomMargin];

    // The navigation bar will set the rightBarButtonItem's height to the full
    // height of the bar. We don't want that for the button so we use a UIView
    // here to contain the button instead and the button is vertically centered
    // inside the full bar height.
    UIView* buttonView =
        [[[UIView alloc] initWithFrame:CGRectZero] autorelease];
    [buttonView addSubview:_payButton];
    // Navigation bar button items are aligned with the trailing edge of the
    // screen. Make the enclosing view larger here. The pay button will be
    // aligned with the leading edge of the enclosing view leaving an inset on
    // the trailing edge.
    CGRect buttonViewBounds = buttonView.bounds;
    buttonViewBounds.size.width =
        [_payButton frame].size.width + kButtonEdgeInset;
    buttonView.bounds = buttonViewBounds;

    UIBarButtonItem* payButtonItem =
        [[[UIBarButtonItem alloc] initWithCustomView:buttonView] autorelease];
    [self navigationItem].rightBarButtonItem = payButtonItem;

    _paymentRequest = paymentRequest;
  }
  return self;
}

- (id<PaymentItemsDisplayViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<PaymentItemsDisplayViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)onReturn {
  [_payButton setEnabled:NO];
  [_delegate paymentItemsDisplayViewControllerDidReturn:self];
}

- (void)onConfirm {
  [_payButton setEnabled:NO];
  [_delegate paymentItemsDisplayViewControllerDidConfirm:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPayment];

  // Add the total entry.
  PriceItem* totalItem =
      [[[PriceItem alloc] initWithType:ItemTypePaymentItemTotal] autorelease];
  totalItem.accessibilityIdentifier = kPaymentItemsDisplayItemId;
  totalItem.item =
      base::SysUTF16ToNSString(_paymentRequest->payment_details().total.label);

  NSString* currencyCode = base::SysUTF16ToNSString(
      _paymentRequest->payment_details().total.amount.currency);
  NSDecimalNumber* value = [NSDecimalNumber
      decimalNumberWithString:SysUTF16ToNSString(
                                  _paymentRequest->payment_details()
                                      .total.amount.value)];
  totalItem.price =
      payment_request_utils::FormattedCurrencyString(value, currencyCode);

  [model addItem:totalItem toSectionWithIdentifier:SectionIdentifierPayment];

  // Add the line item entries.
  for (const auto& paymentItem :
       _paymentRequest->payment_details().display_items) {
    PriceItem* paymentItemItem =
        [[[PriceItem alloc] initWithType:ItemTypePaymentItem] autorelease];
    paymentItemItem.accessibilityIdentifier = kPaymentItemsDisplayItemId;
    paymentItemItem.item = base::SysUTF16ToNSString(paymentItem.label);

    NSString* currencyCode =
        base::SysUTF16ToNSString(paymentItem.amount.currency);
    NSDecimalNumber* value = [NSDecimalNumber
        decimalNumberWithString:SysUTF16ToNSString(paymentItem.amount.value)];
    paymentItemItem.price =
        payment_request_utils::FormattedCurrencyString(value, currencyCode);
    [model addItem:paymentItemItem
        toSectionWithIdentifier:SectionIdentifierPayment];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentItemsDisplayCollectionViewId;

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
    case ItemTypePaymentItemTotal: {
      PriceCell* priceCell = base::mac::ObjCCastStrict<PriceCell>(cell);
      priceCell.itemLabel.font =
          [[MDFRobotoFontLoader sharedInstance] boldFontOfSize:14];
      priceCell.itemLabel.textColor = [[MDCPalette greyPalette] tint600];
      priceCell.priceLabel.font =
          [[MDFRobotoFontLoader sharedInstance] boldFontOfSize:14];
      priceCell.priceLabel.textColor = [[MDCPalette greyPalette] tint900];
      break;
    }
    case ItemTypePaymentItem: {
      PriceCell* priceCell = base::mac::ObjCCastStrict<PriceCell>(cell);
      priceCell.itemLabel.font =
          [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14];
      priceCell.itemLabel.textColor = [[MDCPalette greyPalette] tint900];

      priceCell.priceLabel.font =
          [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14];
      priceCell.priceLabel.textColor = [[MDCPalette greyPalette] tint900];
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark MDCCollectionViewStylingDelegate

// There are no effects from touching the payment items so there should not be
// an ink ripple.
- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

@end
