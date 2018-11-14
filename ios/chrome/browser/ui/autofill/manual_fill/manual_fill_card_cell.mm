// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_cell.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
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
@property(nonatomic, assign) autofill::CreditCard card;

@end

@implementation ManualFillCardItem
@synthesize contentDelegate = _contentDelegate;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize card = _card;

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)card
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

namespace {

// Left and right margins of the cell content.
static const CGFloat sideMargins = 16;

// The multiplier for the base system spacing at the top margin.
static const CGFloat TopSystemSpacingMultiplier = 1.58;

// The multiplier for the base system spacing between elements (vertical).
static const CGFloat MiddleSystemSpacingMultiplier = 1.83;

// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomSystemSpacingMultiplier = 2.26;

// Margin left and right of expiration buttons.
static const CGFloat ExpirationMarginWidth = 16.0;

}  // namespace

@interface ManualFillCardCell ()

// The label with the site name and host.
@property(nonatomic, strong) UILabel* cardLabel;

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
@property(nonatomic, assign) autofill::CreditCard card;

@end

@implementation ManualFillCardCell

@synthesize cardLabel = _cardLabel;
@synthesize cardNumberButton = _cardNumberButton;
@synthesize cardholderButton = _cardholderButton;
@synthesize expirationMonthButton = _expirationMonthButton;
@synthesize expirationYearButton = _expirationYearButton;
@synthesize contentDelegate = _contentDelegate;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize card = _card;

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
}

- (void)setUpWithCreditCard:(const autofill::CreditCard&)card
            contentDelegate:(id<ManualFillContentDelegate>)contentDelegate
         navigationDelegate:(id<CardListDelegate>)navigationDelegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentDelegate = contentDelegate;
  self.navigationDelegate = navigationDelegate;
  self.card = card;

  NSString* cardName = @"";
  if (card.bank_name().empty()) {
    cardName = base::SysUTF16ToNSString(card.NetworkForDisplay());
  } else {
    cardName = base::SysUTF16ToNSString(card.NetworkForDisplay() +
                                        base::ASCIIToUTF16(" ") +
                                        base::ASCIIToUTF16(card.bank_name()));
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

  // Unicode characters used in card number:
  //  - 0x0020 - Space.
  //  - 0x2060 - WORD-JOINER (makes string undivisible).
  constexpr base::char16 separator[] = {0x2060, 0x0020, 0};
  const base::string16 digits = card.LastFourDigits();
  NSString* obfuscatedCardNumber = base::SysUTF16ToNSString(
      autofill::kMidlineEllipsis + base::string16(separator) +
      autofill::kMidlineEllipsis + base::string16(separator) +
      autofill::kMidlineEllipsis + base::string16(separator) + digits);
  [self.cardNumberButton setTitle:obfuscatedCardNumber
                         forState:UIControlStateNormal];

  NSString* cardholder = autofill::GetCreditCardName(
      card, GetApplicationContext()->GetApplicationLocale());
  [self.cardholderButton setTitle:cardholder forState:UIControlStateNormal];

  [self.expirationMonthButton
      setTitle:[NSString stringWithFormat:@"%02d", card.expiration_month()]
      forState:UIControlStateNormal];
  [self.expirationYearButton
      setTitle:[NSString stringWithFormat:@"%04d", card.expiration_year()]
      forState:UIControlStateNormal];
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = UIColor.cr_manualFillGrayLineColor;
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:grayLine];

  self.cardLabel = [self createLabel];
  [self.contentView addSubview:self.cardLabel];
  [self setHorizontalConstraintsForViews:@[ self.cardLabel ]
                                   guide:grayLine
                                   shift:0];

  self.cardNumberButton =
      [self createButtonForAction:@selector(userDidTapCardNumber:)];
  [self.contentView addSubview:self.cardNumberButton];
  [self setHorizontalConstraintsForViews:@[ self.cardNumberButton ]
                                   guide:grayLine
                                   shift:0];

  self.cardholderButton =
      [self createButtonForAction:@selector(userDidTapCardInfo:)];
  [self.contentView addSubview:self.cardholderButton];
  [self setHorizontalConstraintsForViews:@[ self.cardholderButton ]
                                   guide:grayLine
                                   shift:0];

  // Expiration line.
  self.expirationMonthButton =
      [self createButtonForAction:@selector(userDidTapCardInfo:)];
  [self setHorizontalMarginConstraintsForButton:self.expirationMonthButton
                                          width:ExpirationMarginWidth];
  [self.contentView addSubview:self.expirationMonthButton];
  self.expirationYearButton =
      [self createButtonForAction:@selector(userDidTapCardInfo:)];
  [self setHorizontalMarginConstraintsForButton:self.expirationYearButton
                                          width:ExpirationMarginWidth];
  [self.contentView addSubview:self.expirationYearButton];
  UILabel* expirationSeparatorLabel = [self createLabel];
  expirationSeparatorLabel.text = @"/";
  [self.contentView addSubview:expirationSeparatorLabel];
  [self syncBaselinesForViews:@[
    expirationSeparatorLabel, self.expirationYearButton
  ]
                       onView:self.expirationMonthButton];
  [self setHorizontalConstraintsForViews:@[
    self.expirationMonthButton, expirationSeparatorLabel,
    self.expirationYearButton
  ]
                                   guide:grayLine
                                   shift:-ExpirationMarginWidth];

  [self setVerticalSpacingConstraintsForViews:@[
    self.cardLabel, self.cardNumberButton, self.cardholderButton,
    self.expirationMonthButton
  ]
                                    container:self.contentView];

  id<LayoutGuideProvider> safeArea = self.contentView.safeAreaLayoutGuide;

  [NSLayoutConstraint activateConstraints:@[
    // Common vertical constraints.
    [grayLine.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],

    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                           constant:sideMargins],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:sideMargins],
  ]];
}

- (void)userDidTapCardNumber:(UIButton*)sender {
  if (self.card.record_type() == autofill::CreditCard::MASKED_SERVER_CARD) {
    [self.navigationDelegate requestFullCreditCard:self.card];
  } else {
    [self.contentDelegate
        userDidPickContent:base::SysUTF16ToNSString(
                               autofill::CreditCard::StripSeparators(
                                   self.card.GetRawInfo(
                                       autofill::CREDIT_CARD_NUMBER)))
                  isSecure:NO];
  }
}

- (void)userDidTapCardInfo:(UIButton*)sender {
  [self.contentDelegate userDidPickContent:sender.titleLabel.text isSecure:NO];
}

// Creates a blank button for the given |action|.
- (UIButton*)createButtonForAction:(SEL)action {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitleColor:UIColor.cr_manualFillTintColor
               forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Creates horizontal constraints for given |button| based on given |width| on
// both sides.
- (void)setHorizontalMarginConstraintsForButton:(UIButton*)button
                                          width:(CGFloat)width {
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor
        constraintEqualToAnchor:button.titleLabel.leadingAnchor
                       constant:-width],
    [button.trailingAnchor
        constraintEqualToAnchor:button.titleLabel.trailingAnchor
                       constant:width],
  ]];
}

// Sets vertical layout for the button or labels rows in |views| inside
// |container|.
- (void)setVerticalSpacingConstraintsForViews:(NSArray<UIView*>*)views
                                    container:(UIView*)container {
  NSMutableArray* verticalConstraints = [[NSMutableArray alloc] init];
  // Multipliers of these constraints are calculated based on a 24 base
  // system spacing.
  NSLayoutYAxisAnchor* previousAnchor = container.topAnchor;
  CGFloat multiplier = TopSystemSpacingMultiplier;
  for (UIView* view in views) {
    [verticalConstraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                     multiplier:multiplier]];
    multiplier = MiddleSystemSpacingMultiplier;
    previousAnchor = view.lastBaselineAnchor;
  }
  multiplier = BottomSystemSpacingMultiplier;
  [verticalConstraints
      addObject:[container.bottomAnchor
                    constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                   multiplier:multiplier]];
  [NSLayoutConstraint activateConstraints:verticalConstraints];
}

// Sets constraints for the given |views|, so at to lay them out horizontally,
// parallel to the given |guide| view, and applying the given constant |shift|
// to the whole row.
- (void)setHorizontalConstraintsForViews:(NSArray<UIView*>*)views
                                   guide:(UIView*)guide
                                   shift:(CGFloat)shift {
  NSMutableArray* horizontalConstraints = [[NSMutableArray alloc] init];
  NSLayoutXAxisAnchor* previousAnchor = guide.leadingAnchor;
  for (UIView* view in views) {
    [horizontalConstraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previousAnchor
                                                     constant:shift]];
    previousAnchor = view.trailingAnchor;
    shift = 0;
  }
  if (views.count > 0) {
    [horizontalConstraints
        addObject:[views.lastObject.trailingAnchor
                      constraintLessThanOrEqualToAnchor:guide.trailingAnchor
                                               constant:shift]];
  }
  [NSLayoutConstraint activateConstraints:horizontalConstraints];
}

// Sets all baseline anchors for the gievn |views| to match the one on |onView|.
- (void)syncBaselinesForViews:(NSArray<UIView*>*)views onView:(UIView*)onView {
  NSMutableArray* baselinesConstraints = [[NSMutableArray alloc] init];
  for (UIView* view in views) {
    [baselinesConstraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToAnchor:onView.firstBaselineAnchor]];
  }
  [NSLayoutConstraint activateConstraints:baselinesConstraints];
}

// Creates a blank label.
- (UILabel*)createLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

@end
