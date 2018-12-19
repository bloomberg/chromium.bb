// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_cell.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credit_card.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillCardItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentDelegate>
    contentDelegate;

// The navigation delegate for this item.
@property(nonatomic, weak, readonly) id<CardListDelegate> navigationDelegate;

// The credit card for this item.
@property(nonatomic, readonly) ManualFillCreditCard* card;

@end

@implementation ManualFillCardItem
@synthesize contentDelegate = _contentDelegate;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize card = _card;

- (instancetype)initWithCreditCard:(ManualFillCreditCard*)card
                   contentDelegate:
                       (id<ManualFillContentDelegate>)contentDelegate
                navigationDelegate:(id<CardListDelegate>)navigationDelegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _contentDelegate = contentDelegate;
    _navigationDelegate = navigationDelegate;
    _card = card;
    self.cellClass = [ManualFillCardCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillCardCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCreditCard:self.card
            contentDelegate:self.contentDelegate
         navigationDelegate:self.navigationDelegate];
}

@end

@interface ManualFillCardCell ()

// The label with bank name and network.
@property(nonatomic, strong) UILabel* cardLabel;

// The credit card icon.
@property(nonatomic, strong) UIImageView* cardIcon;

// A button showing the card number.
@property(nonatomic, strong) UIButton* cardNumberButton;

// A button showing the cardholder name.
@property(nonatomic, strong) UIButton* cardholderButton;

// A button showing the expiration month.
@property(nonatomic, strong) UIButton* expirationMonthButton;

// A button showing the expiration year.
@property(nonatomic, strong) UIButton* expirationYearButton;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentDelegate> contentDelegate;

// The navigation delegate for this item.
@property(nonatomic, weak) id<CardListDelegate> navigationDelegate;

// The credit card data for this cell.
@property(nonatomic, assign) ManualFillCreditCard* card;

@end

@implementation ManualFillCardCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  self.cardLabel.text = @"";
  [self.cardNumberButton setTitle:@"" forState:UIControlStateNormal];
  [self.cardholderButton setTitle:@"" forState:UIControlStateNormal];
  [self.expirationMonthButton setTitle:@"" forState:UIControlStateNormal];
  [self.expirationYearButton setTitle:@"" forState:UIControlStateNormal];
  self.contentDelegate = nil;
  self.navigationDelegate = nil;
  self.cardIcon.image = nil;
  self.card = nil;
}

- (void)setUpWithCreditCard:(ManualFillCreditCard*)card
            contentDelegate:(id<ManualFillContentDelegate>)contentDelegate
         navigationDelegate:(id<CardListDelegate>)navigationDelegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentDelegate = contentDelegate;
  self.navigationDelegate = navigationDelegate;
  self.card = card;

  NSString* cardName = @"";
  if (card.bankName.length) {
    cardName = card.network;
  } else {
    cardName =
        [NSString stringWithFormat:@"%@ %@", card.network, card.bankName];
  }

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:cardName
              attributes:@{
                NSForegroundColorAttributeName : UIColor.blackColor,
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
  self.cardLabel.attributedText = attributedString;

  self.cardIcon.image = NativeImage(card.issuerNetworkIconID);

  [self.cardNumberButton setTitle:card.obfuscatedNumber
                         forState:UIControlStateNormal];
  [self.cardholderButton setTitle:card.cardHolder
                         forState:UIControlStateNormal];

  [self.expirationMonthButton setTitle:card.expirationMonth
                              forState:UIControlStateNormal];
  [self.expirationYearButton setTitle:card.expirationYear
                             forState:UIControlStateNormal];
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* guide = self.contentView;
  CreateGraySeparatorForContainer(guide);

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.cardLabel = CreateLabel();
  [self.contentView addSubview:self.cardLabel];

  self.cardIcon = [[UIImageView alloc] init];
  self.cardIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.cardIcon];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.cardLabel, self.cardIcon ], guide,
      ButtonHorizontalMargin, AppendConstraintsHorizontalExtraSpaceLeft);
  [NSLayoutConstraint activateConstraints:@[
    [self.cardIcon.bottomAnchor
        constraintEqualToAnchor:self.cardLabel.firstBaselineAnchor]
  ]];

  self.cardNumberButton =
      CreateButtonWithSelectorAndTarget(@selector(userDidTapCardNumber:), self);
  [self.contentView addSubview:self.cardNumberButton];
  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.cardNumberButton ], guide);

  self.cardholderButton =
      CreateButtonWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
  [self.contentView addSubview:self.cardholderButton];
  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.cardholderButton ], guide);

  // Expiration line.
  self.expirationMonthButton =
      CreateButtonWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
  [self.contentView addSubview:self.expirationMonthButton];
  self.expirationYearButton =
      CreateButtonWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
  [self.contentView addSubview:self.expirationYearButton];
  UILabel* expirationSeparatorLabel = CreateLabel();
  expirationSeparatorLabel.text = @"/";
  [self.contentView addSubview:expirationSeparatorLabel];
  AppendHorizontalConstraintsForViews(
      staticConstraints,
      @[
        self.expirationMonthButton, expirationSeparatorLabel,
        self.expirationYearButton
      ],
      guide, 0, AppendConstraintsHorizontalSyncBaselines);

  AppendVerticalConstraintsSpacingForViews(
      staticConstraints,
      @[
        self.cardLabel, self.cardNumberButton, self.expirationMonthButton,
        self.cardholderButton
      ],
      self.contentView);

  // Without this set, Voice Over will read the content vertically instead of
  // horizontally.
  self.contentView.shouldGroupAccessibilityChildren = YES;

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapCardNumber:(UIButton*)sender {
  NSString* number = self.card.number;
  if (![self.contentDelegate canUserInjectInPasswordField:NO
                                            requiresHTTPS:YES]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_CreditCard_SelectCardNumber"));
  if (!number.length) {
    [self.navigationDelegate requestFullCreditCard:self.card];
  } else {
    [self.contentDelegate userDidPickContent:number
                               passwordField:NO
                               requiresHTTPS:YES];
  }
}

- (void)userDidTapCardInfo:(UIButton*)sender {
  const char* metricsAction = nullptr;
  if (sender == self.cardholderButton) {
    metricsAction = "ManualFallback_CreditCard_SelectCardholderName";
  } else if (sender == self.expirationMonthButton) {
    metricsAction = "ManualFallback_CreditCard_SelectExpirationMonth";
  } else if (sender == self.expirationYearButton) {
    metricsAction = "ManualFallback_CreditCard_SelectExpirationYear";
  }
  DCHECK(metricsAction);
  base::RecordAction(base::UserMetricsAction(metricsAction));

  [self.contentDelegate userDidPickContent:sender.titleLabel.text
                             passwordField:NO
                             requiresHTTPS:NO];
}

@end
