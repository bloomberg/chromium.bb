// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "ios/chrome/search_widget_extension/copied_url_view.h"
#import "ios/chrome/search_widget_extension/search_action_view.h"
#import "ios/chrome/search_widget_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kMaxContentSize = 421;

}  // namespace

@interface SearchWidgetView ()
// The content displayed in the actions section.
@property(nonatomic, strong) UIView* actionsContent;
// The actions section. Can be bigger than the content within.
@property(nonatomic, strong) UIView* actionsSection;
// The copied URL section. Fits its contents.
@property(nonatomic, strong) CopiedURLView* copiedURLSection;
// The height used in the compact display mode.
@property(nonatomic) CGFloat compactHeight;
// The target for actions in the view.
@property(nonatomic, weak) id<SearchWidgetViewActionTarget> target;
// The actions section height constraint. Set its constant to modify the action
// section's height.
@property(nonatomic, strong) NSLayoutConstraint* actionsSectionHeightConstraint;

// Sets up the widget UI for an expanded or compact appearance based on
// |compact|.
- (void)createUI:(BOOL)compact;

// Creates the views for the action buttons.
- (void)createActionsView;

// Returns the height of the action content.
- (CGFloat)actionContentHeight;

// Returns the height of the copied URL section.
- (CGFloat)copiedURLSectionHeight;

// Returns the height to use for the action section, depending on the display
// mode.
- (CGFloat)actionSectionHeight:(BOOL)compact;

@end

@implementation SearchWidgetView

@synthesize target = _target;
@synthesize copiedURLSection = _copiedURLSection;
@synthesize actionsSection = _actionsSection;
@synthesize actionsContent = _actionsContent;
@synthesize compactHeight = _compactHeight;
@synthesize actionsSectionHeightConstraint = _actionsSectionHeightConstraint;

- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target
                       compactHeight:(CGFloat)compactHeight
                    initiallyCompact:(BOOL)compact {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;
    _compactHeight = compactHeight;
    [self createUI:compact];
  }
  return self;
}

#pragma mark - UI creation

- (void)createUI:(BOOL)compact {
  [self createActionsView];
  _actionsSection.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_actionsSection];

  _copiedURLSection =
      [[CopiedURLView alloc] initWithActionTarget:self.target
                                   actionSelector:@selector(openCopiedURL:)];
  [self addSubview:_copiedURLSection];

  _actionsSectionHeightConstraint = [self.actionsSection.heightAnchor
      constraintEqualToConstant:[self actionSectionHeight:compact]];

  [NSLayoutConstraint activateConstraints:@[
    [_actionsSection.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_actionsSection.bottomAnchor
        constraintEqualToAnchor:_copiedURLSection.topAnchor],

    [self.leadingAnchor
        constraintEqualToAnchor:self.actionsSection.leadingAnchor],
    [self.leadingAnchor
        constraintEqualToAnchor:self.copiedURLSection.leadingAnchor],
    [self.trailingAnchor
        constraintEqualToAnchor:self.actionsSection.trailingAnchor],
    [self.trailingAnchor
        constraintEqualToAnchor:self.copiedURLSection.trailingAnchor],
    _actionsSectionHeightConstraint,
  ]];
}

- (CGFloat)actionSectionHeight:(BOOL)compact {
  if (compact) {
    return self.compactHeight;
  }
  CGFloat contentHeight = [self actionContentHeight];
  CGFloat copiedURLHeight = [self copiedURLSectionHeight];
  CGFloat height = contentHeight + copiedURLHeight;
  if (height >= self.compactHeight) {
    return contentHeight;
  }
  return self.compactHeight - copiedURLHeight;
}

- (CGFloat)actionContentHeight {
  CGFloat height =
      [self.actionsContent
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
  return height + 2 * ui_util::kContentMargin;
}

- (CGFloat)copiedURLSectionHeight {
  CGFloat height =
      [self.copiedURLSection
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
  return height;
}

- (void)createActionsView {
  UIStackView* actionsContentStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        [[SearchActionView alloc]
            initWithActionTarget:self.target
                  actionSelector:@selector(openSearch:)
                           title:NSLocalizedString(@"IDS_IOS_NEW_SEARCH",
                                                   @"New Search")
                       imageName:@"quick_action_search"],
        [[SearchActionView alloc]
            initWithActionTarget:self.target
                  actionSelector:@selector(openIncognito:)
                           title:NSLocalizedString(@"IDS_IOS_INCOGNITO_SEARCH",
                                                   @"Incognito Search")
                       imageName:@"quick_action_incognito_search"],
        [[SearchActionView alloc]
            initWithActionTarget:self.target
                  actionSelector:@selector(openVoice:)
                           title:NSLocalizedString(@"IDS_IOS_VOICE_SEARCH",
                                                   @"Voice Search")
                       imageName:@"quick_action_voice_search"],
        [[SearchActionView alloc]
            initWithActionTarget:self.target
                  actionSelector:@selector(openQRCode:)
                           title:NSLocalizedString(@"IDS_IOS_SCAN_QR_CODE",
                                                   @"Scan QR Code")
                       imageName:@"quick_action_scan_qr_code"],
      ]];

  actionsContentStack.axis = UILayoutConstraintAxisHorizontal;
  actionsContentStack.alignment = UIStackViewAlignmentTop;
  actionsContentStack.distribution = UIStackViewDistributionFillEqually;
  actionsContentStack.spacing = ui_util::kIconSpacing;
  actionsContentStack.layoutMargins = UIEdgeInsetsZero;
  actionsContentStack.layoutMarginsRelativeArrangement = YES;
  actionsContentStack.translatesAutoresizingMaskIntoConstraints = NO;

  self.actionsContent = actionsContentStack;

  self.actionsSection = [[UIView alloc] initWithFrame:CGRectZero];
  self.actionsSection.translatesAutoresizingMaskIntoConstraints = NO;
  [self.actionsSection addSubview:self.actionsContent];

  // These constraints stretch the action row to the full width of the widget.
  // Their priority is < UILayoutPriorityRequired so that they can break when
  // the view is larger than kMaxContentSize.
  NSLayoutConstraint* actionsLeadingConstraint =
      [self.actionsContent.leadingAnchor
          constraintEqualToAnchor:self.actionsSection.leadingAnchor
                         constant:ui_util::kContentMargin];
  actionsLeadingConstraint.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* actionsTrailingConstraint =
      [self.actionsContent.trailingAnchor
          constraintEqualToAnchor:self.actionsSection.trailingAnchor
                         constant:-ui_util::kContentMargin];
  actionsTrailingConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [self.actionsSection.centerYAnchor
        constraintEqualToAnchor:self.actionsContent.centerYAnchor],
    [self.actionsContent.centerXAnchor
        constraintEqualToAnchor:self.actionsSection.centerXAnchor],
    [self.actionsContent.widthAnchor
        constraintLessThanOrEqualToConstant:kMaxContentSize],
    actionsLeadingConstraint,
    actionsTrailingConstraint,
  ]];
}

#pragma mark - SearchWidgetView

- (void)showMode:(BOOL)compact {
  self.actionsSectionHeightConstraint.constant =
      [self actionSectionHeight:compact];
}

- (CGFloat)widgetHeight {
  return [self actionContentHeight] + [self copiedURLSectionHeight];
}

- (void)setCopiedURLString:(NSString*)URL {
  [self.copiedURLSection setCopiedURLString:URL];
}

@end
