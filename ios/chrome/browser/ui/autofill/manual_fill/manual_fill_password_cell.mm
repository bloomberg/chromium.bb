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

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
@property(nonatomic, assign) BOOL isConnectedToPreviousItem;

// The separator line won't show if it is connected to the next password item.
@property(nonatomic, assign) BOOL isConnectedToNextItem;

// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentDelegate> delegate;

@end

@implementation ManualFillCredentialItem
@synthesize delegate = _delegate;
@synthesize credential = _credential;

- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                          delegate:(id<ManualFillContentDelegate>)delegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _isConnectedToPreviousItem = isConnectedToPreviousItem;
    _isConnectedToNextItem = isConnectedToNextItem;
    _delegate = delegate;
    self.cellClass = [ManualFillPasswordCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPasswordCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCredential:self.credential
      isConnectedToPreviousCell:self.isConnectedToPreviousItem
          isConnectedToNextCell:self.isConnectedToNextItem
                       delegate:self.delegate];
}

@end

namespace {

// The multiplier for the base system spacing at the top margin for connected
// cells.
static const CGFloat TopSystemSpacingMultiplierForConnectedCell = 1.28;

// When no extra multiplier is required.
static const CGFloat NoMultiplier = 1.0;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* manualFillCredential;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;

// A button showing "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;

// Separator line between cells, if needed.
@property(nonatomic, strong) UIView* grayLine;

// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentDelegate> delegate;

@end

@implementation ManualFillPasswordCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";

  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.usernameButton setTitleColor:UIColor.cr_manualFillTintColor
                            forState:UIControlStateNormal];

  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.passwordButton.accessibilityLabel = nil;
  self.passwordButton.hidden = NO;

  self.manualFillCredential = nil;

  self.grayLine.hidden = NO;
}

- (void)setUpWithCredential:(ManualFillCredential*)credential
    isConnectedToPreviousCell:(BOOL)isConnectedToPreviousCell
        isConnectedToNextCell:(BOOL)isConnectedToNextCell
                     delegate:(id<ManualFillContentDelegate>)delegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.delegate = delegate;
  self.manualFillCredential = credential;

  NSMutableArray<UIView*>* verticalLeadViews = [[NSMutableArray alloc] init];

  if (isConnectedToPreviousCell) {
    self.siteNameLabel.hidden = YES;
  } else {
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
    [verticalLeadViews addObject:self.siteNameLabel];
    self.siteNameLabel.hidden = NO;
  }

  if (credential.username.length) {
    [self.usernameButton setTitle:credential.username
                         forState:UIControlStateNormal];
  } else {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_USERNAME);
    [self.usernameButton setTitle:titleString forState:UIControlStateNormal];
    [self.usernameButton setTitleColor:UIColor.lightGrayColor
                              forState:UIControlStateNormal];
    self.usernameButton.enabled = NO;
  }
  [verticalLeadViews addObject:self.usernameButton];

  if (credential.password.length) {
    [self.passwordButton setTitle:@"••••••••" forState:UIControlStateNormal];
    self.passwordButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
    [verticalLeadViews addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  if (isConnectedToNextCell) {
    self.grayLine.hidden = YES;
  }

  CGFloat topSystemSpacingMultiplier =
      isConnectedToPreviousCell ? TopSystemSpacingMultiplierForConnectedCell
                                : TopSystemSpacingMultiplier;
  CGFloat bottomSystemSpacingMultiplier =
      isConnectedToNextCell ? NoMultiplier : BottomSystemSpacingMultiplier;

  self.dynamicConstraints = [[NSMutableArray alloc] init];
  AppendVerticalConstraintsSpacingForViews(
      self.dynamicConstraints, verticalLeadViews, self.contentView,
      topSystemSpacingMultiplier, MiddleSystemSpacingMultiplier,
      bottomSystemSpacingMultiplier);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* guide = self.contentView;
  self.grayLine = CreateGraySeparatorForContainer(guide);
  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.siteNameLabel = CreateLabel();
  self.siteNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.siteNameLabel.adjustsFontForContentSizeCategory = YES;
  [self.contentView addSubview:self.siteNameLabel];
  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.siteNameLabel ], guide,
                                      ButtonHorizontalMargin);

  self.usernameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.usernameButton ], guide);

  self.passwordButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.passwordButton ], guide);

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectUsername"));
  [self.delegate userDidPickContent:self.manualFillCredential.username
                      passwordField:NO
                      requiresHTTPS:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  if (![self.delegate canUserInjectInPasswordField:YES requiresHTTPS:YES]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectPassword"));
  [self.delegate userDidPickContent:self.manualFillCredential.password
                      passwordField:YES
                      requiresHTTPS:YES];
}

@end
