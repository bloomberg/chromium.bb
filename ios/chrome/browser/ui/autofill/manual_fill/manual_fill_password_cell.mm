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

// Left and right margins of the cell content.
static const CGFloat SideMargins = 16;

// The multiplier for the base system spacing at the top margin for connected
// cells.
static const CGFloat TopSystemSpacingMultiplierForConnectedCell = 1.28;

// When no extra multiplier is required.
static const CGFloat NoMultiplier = 1.0;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* manualFillCredential;

// The vertical constraints for all the lines.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* verticalConstraints;

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
  self.siteNameLabel.text = @"";
  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.manualFillCredential = nil;
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
    [verticalLeadViews addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  if (!isConnectedToNextCell) {
    [verticalLeadViews addObject:self.grayLine];
    self.grayLine.hidden = NO;
  } else {
    self.grayLine.hidden = YES;
  }

  CGFloat topSystemSpacingMultiplier =
      isConnectedToPreviousCell ? TopSystemSpacingMultiplierForConnectedCell
                                : TopSystemSpacingMultiplier;
  CGFloat bottomSystemSpacingMultiplier =
      isConnectedToNextCell ? NoMultiplier : BottomSystemSpacingMultiplier;

  self.verticalConstraints =
      VerticalConstraintsSpacingForViewsInContainerWithMultipliers(
          verticalLeadViews, self.contentView, topSystemSpacingMultiplier,
          MiddleSystemSpacingMultiplier, bottomSystemSpacingMultiplier);
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.grayLine = [[UIView alloc] init];
  self.grayLine.backgroundColor = UIColor.cr_manualFillGrayLineColor;
  self.grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.grayLine];

  UIView* guide = self.contentView;

  self.siteNameLabel = CreateLabel();
  self.siteNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.siteNameLabel.adjustsFontForContentSizeCategory = YES;
  [self.contentView addSubview:self.siteNameLabel];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.siteNameLabel ], guide,
                                                SideMargins);

  self.usernameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.usernameButton ], guide,
                                                0);

  self.passwordButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.passwordButton ], guide,
                                                0);

  id<LayoutGuideProvider> safeArea = self.contentView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Common vertical constraints.
    [self.grayLine.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.grayLine.heightAnchor constraintEqualToConstant:1],

    // Horizontal constraints.
    [self.grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                                constant:SideMargins],
    [safeArea.trailingAnchor
        constraintEqualToAnchor:self.grayLine.trailingAnchor
                       constant:SideMargins],
  ]];
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectUsername"));
  [self.delegate userDidPickContent:self.manualFillCredential.username
                    isPasswordField:NO
                      requiresHTTPS:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectPassword"));
  [self.delegate userDidPickContent:self.manualFillCredential.password
                    isPasswordField:YES
                      requiresHTTPS:YES];
}

@end
