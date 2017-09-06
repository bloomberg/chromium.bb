// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_cell.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kPadding = 16;
const CGFloat kButtonHeight = 36;
const CGFloat kTitleSubtitleSpace = 8;
const CGFloat kSubtitleButtonsSpace = 8;
const CGFloat kButtonsSpace = 8;

void SetTextWithLineHeight(UILabel* label, NSString* text, CGFloat lineHeight) {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setMinimumLineHeight:lineHeight];
  [paragraphStyle setMaximumLineHeight:lineHeight];
  NSDictionary* attributes = @{NSParagraphStyleAttributeName : paragraphStyle};
  NSAttributedString* attributedString =
      [[NSAttributedString alloc] initWithString:text attributes:attributes];
  label.attributedText = attributedString;
}

}  // namespace

// The view that contains the promo UI: the title, the subtitle, the dismiss and
// the sign in buttons. It is common to all size classes, but can be embedded
// differently within the BookmarkPromoCell.
@interface BookmarkPromoView : UIView

@property(nonatomic, weak) UILabel* titleLabel;
@property(nonatomic, weak) UILabel* subtitleLabel;
@property(nonatomic, weak) MDCFlatButton* signInButton;
@property(nonatomic, weak) MDCFlatButton* dismissButton;

@end

@implementation BookmarkPromoView

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize signInButton = _signInButton;
@synthesize dismissButton = _dismissButton;

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor whiteColor];
    self.accessibilityIdentifier = @"promo_view";

    // The title.
    UILabel* titleLabel = [[UILabel alloc] init];
    _titleLabel = titleLabel;
    _titleLabel.textColor = bookmark_utils_ios::darkTextColor();
    _titleLabel.font = [[MDCTypography fontLoader] mediumFontOfSize:16];
    _titleLabel.numberOfLines = 0;
    SetTextWithLineHeight(_titleLabel,
                          l10n_util::GetNSString(IDS_IOS_BOOKMARK_PROMO_TITLE),
                          20.f);
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_titleLabel];

    // The subtitle.
    UILabel* subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel = subtitleLabel;
    _subtitleLabel.textColor = bookmark_utils_ios::darkTextColor();
    _subtitleLabel.font = [[MDCTypography fontLoader] regularFontOfSize:14];
    _subtitleLabel.numberOfLines = 0;
    SetTextWithLineHeight(
        _subtitleLabel, l10n_util::GetNSString(IDS_IOS_BOOKMARK_PROMO_MESSAGE),
        20.f);
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_subtitleLabel];

    // The sign-in button.
    MDCFlatButton* signInButton = [[MDCFlatButton alloc] init];
    _signInButton = signInButton;
    [_signInButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                             forState:UIControlStateNormal];
    [_signInButton setTitleColor:[UIColor whiteColor]
                        forState:UIControlStateNormal];
    _signInButton.inkColor = [UIColor colorWithWhite:1 alpha:0.2];
    [_signInButton
        setTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_PROMO_SIGN_IN_BUTTON)
        forState:UIControlStateNormal];
    _signInButton.accessibilityIdentifier = @"promo_sign_in_button";
    _signInButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_signInButton];

    // The dismiss button.
    MDCFlatButton* dismissButton = [[MDCFlatButton alloc] init];
    _dismissButton = dismissButton;
    [_dismissButton
        setTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_PROMO_DISMISS_BUTTON)
        forState:UIControlStateNormal];
    [_dismissButton setTitleColor:[[MDCPalette cr_bluePalette] tint500]
                         forState:UIControlStateNormal];
    _dismissButton.accessibilityIdentifier = @"promo_no_thanks_button";
    _dismissButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_dismissButton];
  }
  return self;
}

- (void)updateConstraints {
  if ([[self constraints] count] == 0) {
    NSDictionary* views = @{
      @"title" : self.titleLabel,
      @"subtitle" : self.subtitleLabel,
      @"signIn" : self.signInButton,
      @"dismiss" : self.dismissButton,
    };
    NSDictionary* metrics = @{
      @"p" : @(kPadding),
      @"b" : @(kButtonHeight),
      @"s1" : @(kTitleSubtitleSpace),
      @"s2" : @(kSubtitleButtonsSpace),
      @"s3" : @(kButtonsSpace),
    };
    // clang-format off
    NSArray* constraints = @[
      @"V:|-(p)-[title]-(s1)-[subtitle]-(s2)-[signIn(==b)]-(p)-|",
      @"H:|-(p)-[title]-(>=p)-|",
      @"H:|-(p)-[subtitle]-(>=p)-|",
      @"H:|-(>=p)-[dismiss]-(s3)-[signIn]-(p)-|"
    ];
    // clang-format on
    ApplyVisualConstraintsWithMetricsAndOptions(
        constraints, views, metrics, LayoutOptionForRTLSupport(), self);
    AddSameCenterYConstraint(self, self.signInButton, self.dismissButton);
  }
  [super updateConstraints];
}

@end

@interface BookmarkPromoCell () {
}
@property(nonatomic, weak) BookmarkPromoView* promoView;
@property(nonatomic, strong) NSArray* compactContentViewConstraints;
@property(nonatomic, strong) NSArray* regularContentViewConstraints;
@end

@implementation BookmarkPromoCell

@synthesize delegate = _delegate;
@synthesize promoView = _promoView;
@synthesize compactContentViewConstraints = _compactContentViewConstraints;
@synthesize regularContentViewConstraints = _regularContentViewConstraints;

+ (NSString*)reuseIdentifier {
  return @"BookmarkPromoCell";
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.translatesAutoresizingMaskIntoConstraints = NO;

    BookmarkPromoView* promoView =
        [[BookmarkPromoView alloc] initWithFrame:frame];
    _promoView = promoView;
    [_promoView.signInButton addTarget:self
                                action:@selector(signIn:)
                      forControlEvents:UIControlEventTouchUpInside];
    [_promoView.dismissButton addTarget:self
                                 action:@selector(dismiss:)
                       forControlEvents:UIControlEventTouchUpInside];
    _promoView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_promoView];
    [self updatePromoView];
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updatePromoView];
}

- (void)updateConstraints {
  if ([[self constraints] count] == 0) {
    NSArray* constraints = @[ @"H:|[contentView]|" ];
    NSDictionary* views = @{ @"contentView" : self.contentView };
    ApplyVisualConstraintsWithMetricsAndOptions(
        constraints, views, nil, LayoutOptionForRTLSupport(), self);
  }
  [super updateConstraints];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _delegate = nil;
}

#pragma mark - Private

- (void)updatePromoView {
  if (IsCompact(self)) {
    self.promoView.layer.cornerRadius = 0;
    self.promoView.layer.borderWidth = 0;
    self.promoView.layer.borderColor = NULL;
    [self.contentView removeConstraints:self.regularContentViewConstraints];
    [self.contentView addConstraints:self.compactContentViewConstraints];
  } else {
    self.promoView.layer.cornerRadius = 3.0;
    self.promoView.layer.borderWidth = 0.5;
    self.promoView.layer.borderColor = UIColorFromRGB(0xCECECE).CGColor;
    [self.contentView removeConstraints:self.compactContentViewConstraints];
    [self.contentView addConstraints:self.regularContentViewConstraints];
  }
}

- (NSArray*)compactContentViewConstraints {
  if (!_compactContentViewConstraints) {
    NSMutableArray* array = [NSMutableArray array];
    NSDictionary* views = @{ @"promoView" : self.promoView };
    [array
        addObjectsFromArray:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"H:|[promoView]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];
    [array
        addObjectsFromArray:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"V:|[promoView]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];
    _compactContentViewConstraints = [array copy];
  }
  return _compactContentViewConstraints;
}

- (NSArray*)regularContentViewConstraints {
  if (!_regularContentViewConstraints) {
    NSMutableArray* array = [NSMutableArray array];
    NSDictionary* views = @{ @"promoView" : self.promoView };
    [array addObjectsFromArray:
               [NSLayoutConstraint
                   constraintsWithVisualFormat:@"H:|-(p)-[promoView]-(p)-|"
                                       options:0
                                       metrics:@{
                                         @"p" : @(kPadding)
                                       }
                                         views:views]];
    [array addObjectsFromArray:
               [NSLayoutConstraint
                   constraintsWithVisualFormat:@"V:|-(p)-[promoView]-(p)-|"
                                       options:0
                                       metrics:@{
                                         @"p" : @(kPadding / 2.0)
                                       }
                                         views:views]];
    _regularContentViewConstraints = [array copy];
  }
  return _regularContentViewConstraints;
}

#pragma mark - Private actions

- (void)signIn:(id)sender {
  [self.delegate bookmarkPromoCellDidTapSignIn:self];
}

- (void)dismiss:(id)sender {
  [self.delegate bookmarkPromoCellDidTapDismiss:self];
}

@end
