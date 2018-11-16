// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"

#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillCredentialItem ()
// The credential for this item.
@property(nonatomic, strong, readonly) ManualFillCredential* credential;
// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentDelegate> delegate;
@end

@implementation ManualFillCredentialItem
@synthesize delegate = _delegate;
@synthesize credential = _credential;

- (instancetype)initWithCredential:(ManualFillCredential*)credential
                          delegate:(id<ManualFillContentDelegate>)delegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _delegate = delegate;
    self.cellClass = [ManualFillPasswordCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPasswordCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCredential:self.credential delegate:self.delegate];
}

@end

namespace {
// Left and right margins of the cell content.
static const CGFloat sideMargins = 16;
}  // namespace

@interface ManualFillPasswordCell ()
// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* manualFillCredential;
// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;
// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;
// A button showing "••••••••" to resemble a password.
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

- (void)prepareForReuse {
  [super prepareForReuse];
  self.siteNameLabel.text = @"";
  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.manualFillCredential = nil;
}

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

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = UIColor.cr_manualFillGrayLineColor;
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:grayLine];

  self.siteNameLabel = CreateLabel();
  self.siteNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.siteNameLabel.adjustsFontForContentSizeCategory = YES;
  [self.contentView addSubview:self.siteNameLabel];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.siteNameLabel ],
                                                grayLine, 0);

  self.usernameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.usernameButton ],
                                                grayLine, 0);

  self.passwordButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.passwordButton ],
                                                grayLine, 0);

  VerticalConstraintsSpacingForViewsInContainer(
      @[ self.siteNameLabel, self.usernameButton, self.passwordButton ],
      self.contentView);

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

- (void)userDidTapUsernameButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectUsername"));
  [self.delegate userDidPickContent:self.manualFillCredential.username
                           isSecure:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectPassword"));
  [self.delegate userDidPickContent:self.manualFillCredential.password
                           isSecure:YES];
}

@end
