// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"

#include "base/mac/foundation_util.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/payments/cells/page_info_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller_actions.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPaymentRequestCollectionViewID =
    @"kPaymentRequestCollectionViewID";

namespace {
const CGFloat kFooterCellHorizontalPadding = 16;

const CGFloat kButtonEdgeInset = 9;
const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSummary = kSectionIdentifierEnumZero,
  SectionIdentifierShipping,
  SectionIdentifierPayment,
  SectionIdentifierContactInfo,
  SectionIdentifierFooter,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSummaryPageInfo = kItemTypeEnumZero,
  ItemTypeSpinner,
  ItemTypeSummaryTotal,
  ItemTypeShippingHeader,
  ItemTypeShippingAddress,
  ItemTypeShippingOption,
  ItemTypePaymentHeader,
  ItemTypePaymentMethod,
  ItemTypeContactInfoHeader,
  ItemTypeContactInfo,
  ItemTypeFooterText,
};

}  // namespace

@interface PaymentRequestViewController ()<
    CollectionViewFooterLinkDelegate,
    PaymentRequestViewControllerActions> {
  UIBarButtonItem* _cancelButton;
  MDCButton* _payButton;
}

@end

@implementation PaymentRequestViewController

@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize connectionSecure = _connectionSecure;
@synthesize pending = _pending;
@synthesize cancellable = _cancellable;
@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  if ((self = [super initWithLayout:layout
                              style:CollectionViewControllerStyleAppBar])) {
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
    _payButton = [[MDCButton alloc] init];
    [_payButton setTitle:l10n_util::GetNSString(IDS_PAYMENTS_PAY_BUTTON)
                forState:UIControlStateNormal];
    [_payButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]];
    [_payButton setTitleColor:[UIColor whiteColor]
                     forState:UIControlStateNormal];
    [_payButton setTitleColor:[UIColor whiteColor]
                     forState:UIControlStateDisabled];
    [_payButton setInkColor:[UIColor colorWithWhite:1 alpha:0.2]];
    [_payButton addTarget:self
                   action:@selector(onConfirm)
         forControlEvents:UIControlEventTouchUpInside];
    [_payButton sizeToFit];
    [_payButton setTranslatesAutoresizingMaskIntoConstraints:NO];

    // The navigation bar will set the rightBarButtonItem's height to the full
    // height of the bar. We don't want that for the button so we use a UIView
    // here to contain the button where the button is vertically centered inside
    // the full bar height. Also navigation bar button items are aligned with
    // the trailing edge of the screen. Make the enclosing view larger and align
    // the pay button with the leading edge of the enclosing view leaving an
    // inset on the trailing edge.
    UIView* buttonView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0,
                                 _payButton.frame.size.width + kButtonEdgeInset,
                                 _payButton.frame.size.height)];
    [buttonView addSubview:_payButton];
    [NSLayoutConstraint activateConstraints:@[
      [_payButton.leadingAnchor
          constraintEqualToAnchor:buttonView.leadingAnchor],
      [_payButton.centerYAnchor
          constraintEqualToAnchor:buttonView.centerYAnchor],
      [_payButton.trailingAnchor
          constraintEqualToAnchor:buttonView.trailingAnchor
                         constant:-kButtonEdgeInset],
    ]];

    UIBarButtonItem* payButtonItem =
        [[UIBarButtonItem alloc] initWithCustomView:buttonView];
    [self navigationItem].rightBarButtonItem = payButtonItem;
  }
  return self;
}

- (void)onCancel {
  [_delegate paymentRequestViewControllerDidCancel:self];
}

- (void)onConfirm {
  [_delegate paymentRequestViewControllerDidConfirm:self];
}

#pragma mark - Setters

- (void)setDataSource:(id<PaymentRequestViewControllerDataSource>)dataSource {
  _dataSource = dataSource;
  [_payButton setEnabled:[_dataSource canPay]];
}

- (void)setCancellable:(BOOL)cancellable {
  _cancellable = cancellable;
  [_cancelButton setEnabled:_cancellable];
  self.view.userInteractionEnabled = cancellable;
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
  pageInfo.connectionSecure = _connectionSecure;
  [model addItem:pageInfo toSectionWithIdentifier:SectionIdentifierSummary];

  if (_pending) {
    [_payButton setEnabled:NO];

    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_PROCESSING_MESSAGE);
    [model addItem:statusItem toSectionWithIdentifier:SectionIdentifierSummary];
    return;
  }

  [self addPaymentSummaryItem];

  // Shipping section.
  if ([_dataSource requestShipping]) {
    [model addSectionWithIdentifier:SectionIdentifierShipping];

    PaymentsTextItem* shippingSectionHeaderItem =
        [_dataSource shippingSectionHeaderItem];
    [shippingSectionHeaderItem setTextColor:[[MDCPalette greyPalette] tint500]];
    [shippingSectionHeaderItem setType:ItemTypeShippingHeader];
    [model setHeader:shippingSectionHeaderItem
        forSectionWithIdentifier:SectionIdentifierShipping];

    [self populateShippingSection];
  }

  // Payment method section.
  [model addSectionWithIdentifier:SectionIdentifierPayment];
  [self populatePaymentMethodSection];

  // Contact Info section.
  if ([_dataSource requestContactInfo]) {
    [model addSectionWithIdentifier:SectionIdentifierContactInfo];
    [self populateContactInfoSection];
  }

  // Footer Text section.
  [model addSectionWithIdentifier:SectionIdentifierFooter];

  CollectionViewFooterItem* footerItem = [_dataSource footerItem];
  [footerItem setType:ItemTypeFooterText];
  footerItem.linkDelegate = self;
  [model addItem:footerItem toSectionWithIdentifier:SectionIdentifierFooter];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier = kPaymentRequestCollectionViewID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
}

- (void)updatePaymentSummaryItem {
  CollectionViewModel* model = self.collectionViewModel;

  [model removeItemWithType:ItemTypeSummaryTotal
      fromSectionWithIdentifier:SectionIdentifierSummary];

  [self addPaymentSummaryItem];

  // Reload the item.
  NSIndexPath* indexPath =
      [model indexPathForItemType:ItemTypeSummaryTotal
                sectionIdentifier:SectionIdentifierSummary];
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)updateShippingSection {
  CollectionViewModel* model = self.collectionViewModel;

  [model removeItemWithType:ItemTypeShippingAddress
      fromSectionWithIdentifier:SectionIdentifierShipping];

  if ([model hasItemForItemType:ItemTypeShippingOption
              sectionIdentifier:SectionIdentifierShipping]) {
    [model removeItemWithType:ItemTypeShippingOption
        fromSectionWithIdentifier:SectionIdentifierShipping];
  }

  [self populateShippingSection];

  // Reload the section.
  NSInteger sectionIndex =
      [model sectionForSectionIdentifier:SectionIdentifierShipping];
  [self.collectionView
      reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]];

  // Update the pay button.
  [_payButton setEnabled:[_dataSource canPay]];
}

- (void)updatePaymentMethodSection {
  CollectionViewModel* model = self.collectionViewModel;

  [model removeItemWithType:ItemTypePaymentMethod
      fromSectionWithIdentifier:SectionIdentifierPayment];

  [self populatePaymentMethodSection];

  // Reload the section.
  NSInteger sectionIndex =
      [model sectionForSectionIdentifier:SectionIdentifierPayment];
  [self.collectionView
      reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]];

  // Update the pay button.
  [_payButton setEnabled:[_dataSource canPay]];
}

- (void)updateContactInfoSection {
  CollectionViewModel* model = self.collectionViewModel;

  [model removeItemWithType:ItemTypeContactInfo
      fromSectionWithIdentifier:SectionIdentifierContactInfo];

  [self populateContactInfoSection];

  // Reload the section.
  NSInteger sectionIndex =
      [model sectionForSectionIdentifier:SectionIdentifierContactInfo];
  [self.collectionView
      reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]];

  // Update the pay button.
  [_payButton setEnabled:[_dataSource canPay]];
}

#pragma mark - CollectionViewFooterLinkDelegate

- (void)cell:(CollectionViewFooterCell*)cell didTapLinkURL:(GURL)url {
  [_delegate paymentRequestViewControllerDidSelectSettings:self];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeShippingAddress:
    case ItemTypePaymentMethod:
    case ItemTypeShippingOption:
    case ItemTypeContactInfo: {
      if ([cell isKindOfClass:[CollectionViewDetailCell class]]) {
        CollectionViewDetailCell* detailCell =
            base::mac::ObjCCastStrict<CollectionViewDetailCell>(cell);
        detailCell.detailTextLabel.font = [MDCTypography body2Font];
        detailCell.detailTextLabel.textColor =
            [[MDCPalette cr_bluePalette] tint500];
      }
      break;
    }
    case ItemTypeFooterText: {
      CollectionViewFooterCell* footerCell =
          base::mac::ObjCCastStrict<CollectionViewFooterCell>(cell);
      footerCell.textLabel.font = [MDCTypography body2Font];
      footerCell.textLabel.textColor = [[MDCPalette greyPalette] tint600];
      footerCell.textLabel.shadowColor = nil;  // No shadow.
      footerCell.horizontalPadding = kFooterCellHorizontalPadding;
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSummaryTotal:
        [_delegate
            paymentRequestViewControllerDidSelectPaymentSummaryItem:self];
      break;
    case ItemTypeShippingAddress:
      [_delegate paymentRequestViewControllerDidSelectShippingAddressItem:self];
      break;
    case ItemTypeShippingOption:
      [_delegate paymentRequestViewControllerDidSelectShippingOptionItem:self];
      break;
    case ItemTypePaymentMethod:
      [_delegate paymentRequestViewControllerDidSelectPaymentMethodItem:self];
      break;
    case ItemTypeContactInfo:
      [_delegate paymentRequestViewControllerDidSelectContactInfoItem:self];
      break;
    case ItemTypeFooterText:
      // Selecting the footer item should not trigger an action, unless the
      // link was clicked, which will call didTapLinkURL:.
      break;
    case ItemTypeSummaryPageInfo:
      // Selecting the page info item should not trigger an action.
      break;
    default:
      NOTREACHED();
      break;
  }
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  // If there are no payment items to display, there is no effect from touching
  // the total so there should not be an ink ripple. The footer and the page
  // info items should also not have a ripple.
  if ((type == ItemTypeSummaryTotal && ![_dataSource hasPaymentItems]) ||
      type == ItemTypeFooterText || type == ItemTypeSummaryPageInfo) {
    return YES;
  } else {
    return NO;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  // No background on the footer text item.
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  return sectionIdentifier == SectionIdentifierFooter ? YES : NO;
}

#pragma mark - Helper methods

- (void)addPaymentSummaryItem {
  CollectionViewItem* item = [_dataSource paymentSummaryItem];
  [item setType:ItemTypeSummaryTotal];
  if ([_dataSource hasPaymentItems])
    item.accessibilityTraits |= UIAccessibilityTraitButton;
  [self.collectionViewModel addItem:item
            toSectionWithIdentifier:SectionIdentifierSummary];
}

- (void)populateShippingSection {
  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem* shippingAddressItem = [_dataSource shippingAddressItem];
  [shippingAddressItem setType:ItemTypeShippingAddress];
  shippingAddressItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:shippingAddressItem
      toSectionWithIdentifier:SectionIdentifierShipping];

  if ([_dataSource canShip]) {
    CollectionViewItem* shippingOptionItem = [_dataSource shippingOptionItem];
    [shippingOptionItem setType:ItemTypeShippingOption];
    shippingOptionItem.accessibilityTraits |= UIAccessibilityTraitButton;
    [model addItem:shippingOptionItem
        toSectionWithIdentifier:SectionIdentifierShipping];
  }
}

- (void)populatePaymentMethodSection {
  CollectionViewModel* model = self.collectionViewModel;

  PaymentsTextItem* paymentMethodSectionHeaderItem =
      [_dataSource paymentMethodSectionHeaderItem];
  if (paymentMethodSectionHeaderItem) {
    [paymentMethodSectionHeaderItem setType:ItemTypePaymentHeader];
    [paymentMethodSectionHeaderItem
        setTextColor:[[MDCPalette greyPalette] tint500]];
    [model setHeader:paymentMethodSectionHeaderItem
        forSectionWithIdentifier:SectionIdentifierPayment];
  }

  CollectionViewItem* paymentMethodItem = [_dataSource paymentMethodItem];
  [paymentMethodItem setType:ItemTypePaymentMethod];
  paymentMethodItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:paymentMethodItem
      toSectionWithIdentifier:SectionIdentifierPayment];
}

- (void)populateContactInfoSection {
  CollectionViewModel* model = self.collectionViewModel;

  PaymentsTextItem* contactInfoSectionHeaderItem =
      [_dataSource contactInfoSectionHeaderItem];
  if (contactInfoSectionHeaderItem) {
    [contactInfoSectionHeaderItem setType:ItemTypeContactInfoHeader];
    [contactInfoSectionHeaderItem
        setTextColor:[[MDCPalette greyPalette] tint500]];
    [model setHeader:contactInfoSectionHeaderItem
        forSectionWithIdentifier:SectionIdentifierContactInfo];
  }

  CollectionViewItem* contactInfoItem = [_dataSource contactInfoItem];
  [contactInfoItem setType:ItemTypeContactInfo];
  contactInfoItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:contactInfoItem
      toSectionWithIdentifier:SectionIdentifierContactInfo];
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self onCancel];
  return YES;
}

@end
