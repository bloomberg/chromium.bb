// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Left and right margins of the cell content.
static const CGFloat sideMargins = 16;
// The base multiplier for the top and bottom margins. This number multiplied by
// the font size plus the base margins will give similar results to
// |constraintEqualToSystemSpacingBelowAnchor:| which is not available on iOS
// 10.
static const CGFloat iOS10MarginFontMultiplier = 1.18;
// The base top margin, only used in iOS 10. Refer to
// |iOS10MarginFontMultiplier| for how it is used.
static const CGFloat iOS10BaseTopMargin = 28;
// The base middle margin, only used in iOS 10. Refer to
// |iOS10MarginFontMultiplier| for how it is used.
static const CGFloat iOS10BaseMiddleMargin = 24;
// The base bottom margin, only used in iOS 10. Refer to
// |iOS10MarginFontMultiplier| for how it is used.
static const CGFloat iOS10BaseBottomMargin = 18;
// The multiplier for the base system spacing at the top margin.
static const CGFloat TopSystemSpacingMultiplier = 1.58;
// The multiplier for the base system spacing between elements (vertical).
static const CGFloat MiddleSystemSpacingMultiplier = 1.83;
// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomSystemSpacingMultiplier = 2.26;
}  // namespace

@interface ManualFillPasswordCell ()
// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* manualFillCredential;
// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;
// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;
// A button showing the "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;
// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentDelegate> delegate;
@end

@implementation ManualFillPasswordCell

@synthesize manualFillCredential = _manualFillCredential;
@synthesize siteNameLabel = _siteNameLabel;
@synthesize usernameButton = _usernameButton;
@synthesize passwordButton = _passwordButton;
@synthesize delegate = _delegate;

#pragma mark - Public

- (void)setUpWithCredential:(ManualFillCredential*)credential
                   delegate:(id<ManualFillContentDelegate>)delegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.delegate = delegate;
  self.manualFillCredential = credential;
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:credential.siteName ? credential.siteName : @""
              attributes:@{
                NSForegroundColorAttributeName : UIColor.blackColor,
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
  if (credential.host && credential.host.length &&
      ![credential.host isEqualToString:credential.siteName]) {
    NSString* hostString =
        [NSString stringWithFormat:@" –– %@", credential.host];
    NSDictionary* attributes = @{
      NSForegroundColorAttributeName : UIColor.lightGrayColor,
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
    };
    NSAttributedString* hostAttributedString =
        [[NSAttributedString alloc] initWithString:hostString
                                        attributes:attributes];
    [attributedString appendAttributedString:hostAttributedString];
  }

  self.siteNameLabel.attributedText = attributedString;
  if (credential.username.length) {
    [self.usernameButton setTitle:credential.username
                         forState:UIControlStateNormal];
  } else {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_USERNAME);
    [self.usernameButton setTitle:titleString forState:UIControlStateNormal];
    self.usernameButton.enabled = NO;
  }

  if (credential.password.length) {
    [self.passwordButton setTitle:@"••••••••" forState:UIControlStateNormal];
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.siteNameLabel.text = @"";
  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.manualFillCredential = nil;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = [UIColor colorWithWhite:0.88 alpha:1];
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:grayLine];

  self.siteNameLabel = [[UILabel alloc] init];
  self.siteNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.siteNameLabel.adjustsFontForContentSizeCategory = YES;
  [self.contentView addSubview:self.siteNameLabel];

  self.usernameButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.usernameButton setTitleColor:UIColor.cr_manualFillTintColor
                            forState:UIControlStateNormal];
  self.usernameButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.usernameButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.usernameButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [self.usernameButton addTarget:self
                          action:@selector(userDidTapUsernameButton:)
                forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.usernameButton];

  self.passwordButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.passwordButton setTitleColor:UIColor.cr_manualFillTintColor
                            forState:UIControlStateNormal];
  self.passwordButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.passwordButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.passwordButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [self.passwordButton addTarget:self
                          action:@selector(userDidTapPasswordButton:)
                forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.passwordButton];

  id<LayoutGuideProvider> safeArea =
      SafeAreaLayoutGuideForView(self.contentView);

  NSArray* verticalConstraints;
  if (@available(iOS 11, *)) {
    // Multipliers of these constraints are calculated based on a 24 base
    // system spacing.
    verticalConstraints = @[
      [self.siteNameLabel.firstBaselineAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.contentView.topAnchor
                                         multiplier:TopSystemSpacingMultiplier],
      [self.usernameButton.firstBaselineAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.siteNameLabel
                                                        .lastBaselineAnchor
                                         multiplier:
                                             MiddleSystemSpacingMultiplier],
      [self.passwordButton.firstBaselineAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.usernameButton
                                                        .lastBaselineAnchor
                                         multiplier:
                                             MiddleSystemSpacingMultiplier],
      [self.contentView.bottomAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.passwordButton
                                                        .lastBaselineAnchor
                                         multiplier:
                                             BottomSystemSpacingMultiplier],
    ];
  } else {
    CGFloat pointSize = self.usernameButton.titleLabel.font.pointSize;
    // These margins are based on the design size and the current point size.
    // The multipliers were selected by manually testing the different system
    // font sizes.
    CGFloat marginBetweenButtons =
        iOS10BaseMiddleMargin + pointSize * iOS10MarginFontMultiplier;
    CGFloat marginBottom =
        iOS10BaseBottomMargin + pointSize * iOS10MarginFontMultiplier / 2;
    CGFloat marginTop =
        iOS10BaseTopMargin + pointSize * iOS10MarginFontMultiplier / 2;

    verticalConstraints = @[
      // This doesn't make sense when the label is to big.
      [self.siteNameLabel.firstBaselineAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:marginTop],
      [self.usernameButton.firstBaselineAnchor
          constraintEqualToAnchor:self.siteNameLabel.lastBaselineAnchor
                         constant:marginBetweenButtons],
      [self.passwordButton.firstBaselineAnchor
          constraintEqualToAnchor:self.usernameButton.lastBaselineAnchor
                         constant:marginBetweenButtons],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:self.passwordButton.lastBaselineAnchor
                         constant:marginBottom],
    ];
  }

  [NSLayoutConstraint activateConstraints:verticalConstraints];
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

    [self.siteNameLabel.leadingAnchor
        constraintEqualToAnchor:grayLine.leadingAnchor],
    [self.siteNameLabel.trailingAnchor
        constraintEqualToAnchor:grayLine.trailingAnchor],
    [self.usernameButton.leadingAnchor
        constraintEqualToAnchor:grayLine.leadingAnchor],
    [self.usernameButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:grayLine.trailingAnchor],
    [self.passwordButton.leadingAnchor
        constraintEqualToAnchor:grayLine.leadingAnchor],
    [self.passwordButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:grayLine.trailingAnchor],
  ]];
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  [self.delegate userDidPickContent:self.manualFillCredential.username
                           isSecure:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  [self.delegate userDidPickContent:self.manualFillCredential.password
                           isSecure:YES];
}

@end
