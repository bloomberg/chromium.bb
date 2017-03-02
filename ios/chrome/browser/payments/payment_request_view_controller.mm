// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_view_controller.h"

#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/payments/cells/page_info_item.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/price_item.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/payments/payment_request_view_controller_actions.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetShippingSectionTitle;
using ::payment_request_util::GetShippingAddressSelectorTitle;
using ::payment_request_util::GetShippingOptionSelectorTitle;

}  // namespace

NSString* const kPaymentRequestCollectionViewID =
    @"kPaymentRequestCollectionViewID";

namespace {

const CGFloat kButtonEdgeInset = 9;
const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSummary = kSectionIdentifierEnumZero,
  SectionIdentifierShipping,
  SectionIdentifierPayment,
  SectionIdentifierContactInfo,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSummaryPageInfo = kItemTypeEnumZero,
  ItemTypeSpinner,
  ItemTypeSummaryTotal,
  ItemTypeShippingTitle,
  ItemTypeShippingAddress,
  ItemTypeAddShippingAddress,
  ItemTypeShippingOption,
  ItemTypeSelectShippingOption,
  ItemTypePaymentTitle,
  ItemTypePaymentMethod,
  ItemTypeAddPaymentMethod,
  ItemTypeContactInfoTitle,
  ItemTypeContactInfo,
  ItemTypeAddContactInfo,
};

}  // namespace

@interface PaymentRequestViewController ()<
    PaymentRequestViewControllerActions> {
  UIBarButtonItem* _cancelButton;
  MDCFlatButton* _payButton;

  // The PaymentRequest object owning an instance of web::PaymentRequest as
  // provided by the page invoking the Payment Request API. This is a weak
  // pointer and should outlive this class.
  PaymentRequest* _paymentRequest;

  __weak PriceItem* _paymentSummaryItem;
  __weak AutofillProfileItem* _selectedShippingAddressItem;
  __weak CollectionViewTextItem* _selectedShippingOptionItem;
  __weak PaymentMethodItem* _selectedPaymentMethodItem;
  __weak AutofillProfileItem* _selectedContactInfoItem;
}

@end

@implementation PaymentRequestViewController

@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize pending = _pending;
@synthesize delegate = _delegate;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest {
  DCHECK(paymentRequest);
  if ((self = [super initWithStyle:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(IDS_PAYMENTS_TITLE)];

    // Set up leading (cancel) button.
    _cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(onCancel)];
    [_cancelButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                                 forState:UIControlStateDisabled];
    [_cancelButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CANCEL)];
    [self navigationItem].leftBarButtonItem = _cancelButton;

    // Set up trailing (pay) button.
    _payButton = [[MDCFlatButton alloc] init];
    [_payButton setTitle:l10n_util::GetNSString(IDS_PAYMENTS_PAY_BUTTON)
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
    [_payButton setEnabled:(paymentRequest->selected_credit_card() != nil)];
    [_payButton setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                                    UIViewAutoresizingFlexibleTopMargin |
                                    UIViewAutoresizingFlexibleBottomMargin];

    // The navigation bar will set the rightBarButtonItem's height to the full
    // height of the bar. We don't want that for the button so we use a UIView
    // here to contain the button instead and the button is vertically centered
    // inside the full bar height.
    UIView* buttonView = [[UIView alloc] initWithFrame:CGRectZero];
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
        [[UIBarButtonItem alloc] initWithCustomView:buttonView];
    [self navigationItem].rightBarButtonItem = payButtonItem;

    _paymentRequest = paymentRequest;
  }
  return self;
}

- (void)onCancel {
  [_delegate paymentRequestViewControllerDidCancel:self];
}

- (void)onConfirm {
  [_delegate paymentRequestViewControllerDidConfirm:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Summary section.
  [model addSectionWithIdentifier:SectionIdentifierSummary];

  PageInfoItem* pageInfo =
      [[PageInfoItem alloc] initWithType:ItemTypeSummaryPageInfo];
  pageInfo.pageFavicon = _pageFavicon;
  pageInfo.pageTitle = _pageTitle;
  pageInfo.pageHost = _pageHost;
  [model setHeader:pageInfo forSectionWithIdentifier:SectionIdentifierSummary];

  if (_pending) {
    [_payButton setEnabled:NO];
    [_cancelButton setEnabled:NO];

    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_PROCESSING_MESSAGE);
    [model addItem:statusItem toSectionWithIdentifier:SectionIdentifierSummary];
    return;
  }

  PriceItem* paymentSummaryItem =
      [[PriceItem alloc] initWithType:ItemTypeSummaryTotal];
  _paymentSummaryItem = paymentSummaryItem;
  [self fillPaymentSummaryItem:paymentSummaryItem
               withPaymentItem:_paymentRequest->payment_details().total
         withTotalValueChanged:NO];
  if (!_paymentRequest->payment_details().display_items.empty()) {
    paymentSummaryItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    paymentSummaryItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  [model addItem:paymentSummaryItem
      toSectionWithIdentifier:SectionIdentifierSummary];

  // Shipping section.
  [model addSectionWithIdentifier:SectionIdentifierShipping];

  CollectionViewTextItem* shippingTitle =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeShippingTitle];
  shippingTitle.text = GetShippingSectionTitle(*_paymentRequest);
  [model setHeader:shippingTitle
      forSectionWithIdentifier:SectionIdentifierShipping];

  CollectionViewItem* shippingAddressItem = nil;
  if (_paymentRequest->selected_shipping_profile()) {
    AutofillProfileItem* selectedShippingAddressItem =
        [[AutofillProfileItem alloc] initWithType:ItemTypeShippingAddress];
    shippingAddressItem = selectedShippingAddressItem;
    _selectedShippingAddressItem = selectedShippingAddressItem;
    [self fillShippingAddressItem:selectedShippingAddressItem
              withAutofillProfile:_paymentRequest->selected_shipping_profile()];
    selectedShippingAddressItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    selectedShippingAddressItem.accessibilityTraits |=
        UIAccessibilityTraitButton;
  } else {
    CollectionViewDetailItem* addAddressItem = [[CollectionViewDetailItem alloc]
        initWithType:ItemTypeAddShippingAddress];
    shippingAddressItem = addAddressItem;
    addAddressItem.text = GetShippingAddressSelectorTitle(*_paymentRequest);
    addAddressItem.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
    addAddressItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  [model addItem:shippingAddressItem
      toSectionWithIdentifier:SectionIdentifierShipping];

  CollectionViewItem* shippingOptionItem = nil;
  if (_paymentRequest->selected_shipping_option()) {
    CollectionViewTextItem* selectedShippingOptionItem =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeShippingOption];
    shippingOptionItem = selectedShippingOptionItem;
    _selectedShippingOptionItem = selectedShippingOptionItem;
    [self fillShippingOptionItem:selectedShippingOptionItem
                      withOption:_paymentRequest->selected_shipping_option()];
    selectedShippingOptionItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    selectedShippingOptionItem.accessibilityTraits |=
        UIAccessibilityTraitButton;
  } else {
    CollectionViewDetailItem* selectShippingOptionItem =
        [[CollectionViewDetailItem alloc]
            initWithType:ItemTypeSelectShippingOption];
    shippingOptionItem = selectShippingOptionItem;
    selectShippingOptionItem.text =
        GetShippingOptionSelectorTitle(*_paymentRequest);
    selectShippingOptionItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    selectShippingOptionItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  [model addItem:shippingOptionItem
      toSectionWithIdentifier:SectionIdentifierShipping];

  // Payment method section.
  [model addSectionWithIdentifier:SectionIdentifierPayment];

  CollectionViewItem* paymentMethodItem = nil;
  if (_paymentRequest->selected_credit_card()) {
    CollectionViewTextItem* paymentTitle =
        [[CollectionViewTextItem alloc] initWithType:ItemTypePaymentTitle];
    paymentTitle.text =
        l10n_util::GetNSString(IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
    [model setHeader:paymentTitle
        forSectionWithIdentifier:SectionIdentifierPayment];

    PaymentMethodItem* selectedPaymentMethodItem =
        [[PaymentMethodItem alloc] initWithType:ItemTypePaymentMethod];
    paymentMethodItem = selectedPaymentMethodItem;
    _selectedPaymentMethodItem = selectedPaymentMethodItem;
    [self fillPaymentMethodItem:selectedPaymentMethodItem
                 withCreditCard:_paymentRequest->selected_credit_card()];
    selectedPaymentMethodItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    selectedPaymentMethodItem.accessibilityTraits |= UIAccessibilityTraitButton;
  } else {
    CollectionViewDetailItem* addPaymentMethodItem = [
        [CollectionViewDetailItem alloc] initWithType:ItemTypeAddPaymentMethod];
    paymentMethodItem = addPaymentMethodItem;
    addPaymentMethodItem.text =
        l10n_util::GetNSString(IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
    addPaymentMethodItem.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
    addPaymentMethodItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  [model addItem:paymentMethodItem
      toSectionWithIdentifier:SectionIdentifierPayment];

  // Contact Info section.
  [model addSectionWithIdentifier:SectionIdentifierContactInfo];

  CollectionViewItem* contactInfoItem = nil;
  if (_paymentRequest->selected_contact_profile()) {
    CollectionViewTextItem* contactInfoTitle =
        [[CollectionViewTextItem alloc] initWithType:ItemTypeContactInfoTitle];
    contactInfoTitle.text =
        l10n_util::GetNSString(IDS_PAYMENTS_CONTACT_DETAILS_LABEL);
    [model setHeader:contactInfoTitle
        forSectionWithIdentifier:SectionIdentifierContactInfo];

    AutofillProfileItem* selectedContactInfoItem =
        [[AutofillProfileItem alloc] initWithType:ItemTypeContactInfo];
    contactInfoItem = selectedContactInfoItem;
    _selectedContactInfoItem = selectedContactInfoItem;
    [self fillContactInfoItem:selectedContactInfoItem
          withAutofillProfile:_paymentRequest->selected_contact_profile()];
    selectedContactInfoItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;

  } else {
    CollectionViewDetailItem* addContactInfoItem =
        [[CollectionViewDetailItem alloc] initWithType:ItemTypeAddContactInfo];
    contactInfoItem = addContactInfoItem;
    addContactInfoItem.text =
        l10n_util::GetNSString(IDS_PAYMENTS_CONTACT_DETAILS_LABEL);
    addContactInfoItem.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
    addContactInfoItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  [model addItem:contactInfoItem
      toSectionWithIdentifier:SectionIdentifierContactInfo];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier = kPaymentRequestCollectionViewID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

- (void)updatePaymentSummaryWithTotalValueChanged:(BOOL)totalValueChanged {
  [self fillPaymentSummaryItem:_paymentSummaryItem
               withPaymentItem:_paymentRequest->payment_details().total
         withTotalValueChanged:totalValueChanged];
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItem:_paymentSummaryItem
                         inSectionWithIdentifier:SectionIdentifierSummary];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)updateSelectedShippingAddressUI {
  [self fillShippingAddressItem:_selectedShippingAddressItem
            withAutofillProfile:_paymentRequest->selected_shipping_profile()];
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItem:_selectedShippingAddressItem
                         inSectionWithIdentifier:SectionIdentifierShipping];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)updateSelectedShippingOptionUI {
  [self fillShippingOptionItem:_selectedShippingOptionItem
                    withOption:_paymentRequest->selected_shipping_option()];
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItem:_selectedShippingOptionItem
                         inSectionWithIdentifier:SectionIdentifierShipping];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)updateSelectedPaymentMethodUI {
  [self fillPaymentMethodItem:_selectedPaymentMethodItem
               withCreditCard:_paymentRequest->selected_credit_card()];
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItem:_selectedPaymentMethodItem
                         inSectionWithIdentifier:SectionIdentifierPayment];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

#pragma mark - Helper methods

- (void)fillPaymentSummaryItem:(PriceItem*)item
               withPaymentItem:(web::PaymentItem)paymentItem
         withTotalValueChanged:(BOOL)totalValueChanged {
  item.item =
      base::SysUTF16ToNSString(_paymentRequest->payment_details().total.label);
  payments::CurrencyFormatter* currencyFormatter =
      _paymentRequest->GetOrCreateCurrencyFormatter();
  item.price = SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(currencyFormatter->formatted_currency_code()),
      currencyFormatter->Format(base::UTF16ToASCII(paymentItem.amount.value))));
  item.notification = totalValueChanged
                          ? l10n_util::GetNSString(IDS_PAYMENTS_UPDATED_LABEL)
                          : nil;
}

- (void)fillShippingAddressItem:(AutofillProfileItem*)item
            withAutofillProfile:(autofill::AutofillProfile*)profile {
  DCHECK(profile);
  item.name = GetNameLabelFromAutofillProfile(*profile);
  item.address = GetAddressLabelFromAutofillProfile(*profile);
  item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
}

- (void)fillShippingOptionItem:(CollectionViewTextItem*)item
                    withOption:(web::PaymentShippingOption*)option {
  item.text = base::SysUTF16ToNSString(option->label);
  payments::CurrencyFormatter* currencyFormatter =
      _paymentRequest->GetOrCreateCurrencyFormatter();
  item.detailText = SysUTF16ToNSString(
      currencyFormatter->Format(base::UTF16ToASCII(option->amount.value)));
}

- (void)fillPaymentMethodItem:(PaymentMethodItem*)item
               withCreditCard:(autofill::CreditCard*)creditCard {
  item.methodID = base::SysUTF16ToNSString(creditCard->TypeAndLastFourDigits());
  item.methodDetail = base::SysUTF16ToNSString(
      creditCard->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  int selectedMethodCardTypeIconID =
      autofill::data_util::GetPaymentRequestData(creditCard->type())
          .icon_resource_id;
  item.methodTypeIcon = NativeImage(selectedMethodCardTypeIconID);
}

- (void)fillContactInfoItem:(AutofillProfileItem*)item
        withAutofillProfile:(autofill::AutofillProfile*)profile {
  DCHECK(profile);
  item.name = GetNameLabelFromAutofillProfile(*profile);
  item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
  item.email = GetEmailLabelFromAutofillProfile(*profile);
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeAddShippingAddress: {
      CollectionViewDetailCell* detailCell =
          base::mac::ObjCCastStrict<CollectionViewDetailCell>(cell);
      detailCell.detailTextLabel.font = [MDCTypography body2Font];
      detailCell.detailTextLabel.textColor =
          [[MDCPalette cr_bluePalette] tint700];
      break;
    }
    case ItemTypeShippingOption: {
      MDCCollectionViewTextCell* textCell =
          base::mac::ObjCCastStrict<MDCCollectionViewTextCell>(cell);
      textCell.textLabel.font = [MDCTypography body2Font];
      textCell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
      textCell.detailTextLabel.font = [MDCTypography body1Font];
      textCell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint900];
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

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSummaryTotal:
      if (!_paymentRequest->payment_details().display_items.empty())
        [_delegate
            paymentRequestViewControllerDidSelectPaymentSummaryItem:self];
      break;
    case ItemTypeShippingAddress:
    case ItemTypeAddShippingAddress:
      [_delegate paymentRequestViewControllerDidSelectShippingAddressItem:self];
      break;
    case ItemTypeShippingOption:
    case ItemTypeSelectShippingOption:
      [_delegate paymentRequestViewControllerDidSelectShippingOptionItem:self];
      break;
    case ItemTypePaymentMethod:
    case ItemTypeAddPaymentMethod:
      [_delegate paymentRequestViewControllerDidSelectPaymentMethodItem:self];
      break;
    case ItemTypeContactInfo:
    case ItemTypeAddContactInfo:
      // TODO(crbug.com/602666): Handle displaying contact info selection view.
      break;
    default:
      NOTREACHED();
      break;
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeSpinner:
    case ItemTypeShippingAddress:
    case ItemTypePaymentMethod:
    case ItemTypeContactInfo:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeShippingOption:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeSummaryPageInfo:
    case ItemTypeSummaryTotal:
    case ItemTypeShippingTitle:
    case ItemTypeAddShippingAddress:
    case ItemTypeSelectShippingOption:
    case ItemTypePaymentTitle:
    case ItemTypeAddPaymentMethod:
    case ItemTypeContactInfoTitle:
    case ItemTypeAddContactInfo:
      return MDCCellDefaultOneLineHeight;
    default:
      NOTREACHED();
      return MDCCellDefaultOneLineHeight;
  }
}

// If there are no payment items to display, there is no effect from touching
// the total so there should not be an ink ripple.
- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeSummaryTotal &&
      _paymentRequest->payment_details().display_items.empty()) {
    return YES;
  } else {
    return NO;
  }
}

@end
