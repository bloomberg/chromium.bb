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

// The copied URL section. Fits its contents.
@property(nonatomic, strong) CopiedURLView* copiedURLSection;
// The target for actions in the view.
@property(nonatomic, weak) id<SearchWidgetViewActionTarget> target;
// The primary effect of the widget. Use for a more opaque appearance.
@property(nonatomic, strong) UIVisualEffect* primaryEffect;
// The secondary effect of the widget. Use for a more transparent appearance.
@property(nonatomic, strong) UIVisualEffect* secondaryEffect;

// Sets up the widget UI.
- (void)createUI;

// Creates the view for the action buttons.
- (UIView*)newActionsView;

@end

@implementation SearchWidgetView

@synthesize target = _target;
@synthesize primaryEffect = _primaryEffect;
@synthesize secondaryEffect = _secondaryEffect;
@synthesize copiedURLSection = _copiedURLSection;

- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target
               primaryVibrancyEffect:(UIVibrancyEffect*)primaryVibrancyEffect
             secondaryVibrancyEffect:
                 (UIVibrancyEffect*)secondaryVibrancyEffect {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;
    _primaryEffect = primaryVibrancyEffect;
    _secondaryEffect = secondaryVibrancyEffect;
    [self createUI];
  }
  return self;
}

#pragma mark - UI creation

- (void)createUI {
  UIView* actionsView = [self newActionsView];
  [self addSubview:actionsView];

  _copiedURLSection =
      [[CopiedURLView alloc] initWithActionTarget:self.target
                                   actionSelector:@selector(openCopiedURL:)
                                    primaryEffect:self.primaryEffect
                                  secondaryEffect:self.secondaryEffect];
  [self addSubview:_copiedURLSection];

  // These constraints stretch the action row to the full width of the widget.
  // Their priority is < UILayoutPriorityRequired so that they can break when
  // the view is larger than kMaxContentSize.
  NSLayoutConstraint* actionsLeadingConstraint =
      [actionsView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor];
  actionsLeadingConstraint.priority = UILayoutPriorityDefaultHigh;

  NSLayoutConstraint* actionsTrailingConstraint =
      [actionsView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor];
  actionsTrailingConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [actionsView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [actionsView.widthAnchor
        constraintLessThanOrEqualToConstant:kMaxContentSize],
    actionsLeadingConstraint,
    actionsTrailingConstraint,
    [actionsView.topAnchor constraintEqualToAnchor:self.topAnchor
                                          constant:ui_util::kContentMargin],
    [actionsView.bottomAnchor
        constraintEqualToAnchor:_copiedURLSection.topAnchor],
    [_copiedURLSection.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [_copiedURLSection.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [_copiedURLSection.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

- (UIView*)newActionsView {
  UIStackView* actionRow = [[UIStackView alloc] initWithArrangedSubviews:@[
    [[SearchActionView alloc]
        initWithActionTarget:self.target
              actionSelector:@selector(openSearch:)
               primaryEffect:self.primaryEffect
             secondaryEffect:self.secondaryEffect
                       title:NSLocalizedString(@"IDS_IOS_NEW_SEARCH",
                                               @"New Search")
                   imageName:@"quick_action_search"],

    [[SearchActionView alloc]
        initWithActionTarget:self.target
              actionSelector:@selector(openIncognito:)
               primaryEffect:self.primaryEffect
             secondaryEffect:self.secondaryEffect
                       title:NSLocalizedString(@"IDS_IOS_INCOGNITO_SEARCH",
                                               @"Incognito Search")
                   imageName:@"quick_action_incognito_search"],
    [[SearchActionView alloc]
        initWithActionTarget:self.target
              actionSelector:@selector(openVoice:)
               primaryEffect:self.primaryEffect
             secondaryEffect:self.secondaryEffect
                       title:NSLocalizedString(@"IDS_IOS_VOICE_SEARCH",
                                               @"Voice Search")
                   imageName:@"quick_action_voice_search"],
    [[SearchActionView alloc]
        initWithActionTarget:self.target
              actionSelector:@selector(openQRCode:)
               primaryEffect:self.primaryEffect
             secondaryEffect:self.secondaryEffect
                       title:NSLocalizedString(@"IDS_IOS_SCAN_QR_CODE",
                                               @"Scan QR Code")
                   imageName:@"quick_action_camera_search"],
  ]];

  actionRow.axis = UILayoutConstraintAxisHorizontal;
  actionRow.alignment = UIStackViewAlignmentTop;
  actionRow.distribution = UIStackViewDistributionFillEqually;
  actionRow.spacing = ui_util::kIconSpacing;
  actionRow.layoutMargins =
      UIEdgeInsetsMake(0, ui_util::kContentMargin, 0, ui_util::kContentMargin);
  actionRow.layoutMarginsRelativeArrangement = YES;
  actionRow.translatesAutoresizingMaskIntoConstraints = NO;
  return actionRow;
}

- (void)setCopiedURLString:(NSString*)URL {
  [self.copiedURLSection setCopiedURLString:URL];
}

@end
