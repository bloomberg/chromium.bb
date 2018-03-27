// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Left margin for search icon.
const CGFloat kSearchIconLeftMargin = 9;

}  // namespace

@interface ContentSuggestionsHeaderView ()<ToolbarSnapshotProviding>

// Layout constraints for fake omnibox background image.
@property(nonatomic, strong) NSLayoutConstraint* backgroundHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* backgroundLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* backgroundTrailingConstraint;

@end

@implementation ContentSuggestionsHeaderView

@synthesize backgroundHeightConstraint = _backgroundHeightConstraint;
@synthesize backgroundLeadingConstraint = _backgroundLeadingConstraint;
@synthesize backgroundTrailingConstraint = _backgroundTrailingConstraint;
@synthesize toolBarView = _toolBarView;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
  }
  return self;
}

#pragma mark - NTPHeaderViewAdapter

- (void)addToolbarView:(UIView*)toolbarView {
  DCHECK(!self.toolBarView);
  _toolBarView = toolbarView;
  [self addSubview:self.toolBarView];
  // TODO(crbug.com/808431) This logic is duplicated in various places and
  // should be refactored away for content suggestions.
  NSArray<GuideName*>* guideNames = @[
    kOmniboxGuide,
    kBackButtonGuide,
    kForwardButtonGuide,
    kToolsMenuGuide,
    kTabSwitcherGuide,
  ];
  AddNamedGuidesToView(guideNames, self);
  [NSLayoutConstraint activateConstraints:@[
    [self.toolBarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.toolBarView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [self.toolBarView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
  UIBlurEffect* blurEffect = buttonFactory.toolbarConfiguration.blurEffect;
  UIVisualEffectView* blur =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blur.layer.cornerRadius = kAdaptiveLocationBarCornerRadius;
  [searchField insertSubview:blur atIndex:0];
  blur.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(blur, searchField);

  UIVisualEffect* vibrancy = [buttonFactory.toolbarConfiguration
      vibrancyEffectForBlurEffect:blurEffect];
  UIVisualEffectView* vibrancyView =
      [[UIVisualEffectView alloc] initWithEffect:vibrancy];
  [searchField insertSubview:vibrancyView atIndex:1];
  vibrancyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(vibrancyView, searchField);

  UIView* backgroundContainer = [[UIView alloc] init];
  backgroundContainer.userInteractionEnabled = NO;
  backgroundContainer.backgroundColor =
      UIColorFromRGB(content_suggestions::kSearchFieldBackgroundColor);
  backgroundContainer.layer.cornerRadius = kAdaptiveLocationBarCornerRadius;
  [vibrancyView.contentView addSubview:backgroundContainer];

  backgroundContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundLeadingConstraint = [backgroundContainer.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.backgroundTrailingConstraint = [backgroundContainer.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.backgroundHeightConstraint = [backgroundContainer.heightAnchor
      constraintEqualToConstant:content_suggestions::kSearchFieldHeight];
  [NSLayoutConstraint activateConstraints:@[
    [backgroundContainer.centerYAnchor
        constraintEqualToAnchor:searchField.centerYAnchor],
    self.backgroundLeadingConstraint,
    self.backgroundTrailingConstraint,
    self.backgroundHeightConstraint,
  ]];

  UIImage* search_icon = [UIImage imageNamed:@"ntp_search_icon"];
  UIImageView* search_view = [[UIImageView alloc] initWithImage:search_icon];
  [vibrancyView.contentView addSubview:search_view];
  search_view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [search_view.centerYAnchor
        constraintEqualToAnchor:backgroundContainer.centerYAnchor],
    [search_view.leadingAnchor
        constraintEqualToAnchor:backgroundContainer.leadingAnchor
                       constant:kSearchIconLeftMargin],
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset =
      self.frame.size.height - ntp_header::kMinHeaderHeight;
  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset && offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = MIN(1, MAX(0, animatingOffset / ntp_header::kAnimationDistance));
  }
  return percent;
}

- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
                     hintLabel:(UILabel*)hintLabel
                hintLabelWidth:(NSLayoutConstraint*)hintLabelWidthConstraint
            subviewConstraints:(NSArray*)constraints
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGFloat contentWidth = std::max<CGFloat>(
      0, screenWidth - safeAreaInsets.left - safeAreaInsets.right);
  if (screenWidth == 0 || contentWidth == 0)
    return;

  CGFloat searchFieldNormalWidth =
      content_suggestions::searchFieldWidth(contentWidth);

  CGFloat percent = [self searchFieldProgressForOffset:offset];
  if (self.cr_widthSizeClass == REGULAR && self.cr_heightSizeClass == REGULAR) {
    self.alpha = 1 - percent;
    widthConstraint.constant = searchFieldNormalWidth;
    hintLabelWidthConstraint.active = NO;
    return;
  } else {
    hintLabelWidthConstraint.active = YES;
    self.alpha = 1;
  }

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  CGFloat maxHeightDiff =
      ntp_header::ToolbarHeight() - content_suggestions::kSearchFieldHeight;

  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::searchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant =
      content_suggestions::kSearchFieldHeight + maxHeightDiff * percent;

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  self.backgroundLeadingConstraint.constant =
      (safeAreaInsets.left + kExpandedLocationBarHorizontalMargin) * percent;
  self.backgroundTrailingConstraint.constant =
      -(safeAreaInsets.right + kExpandedLocationBarHorizontalMargin) * percent;
  // TODO(crbug.com/805645) This should take into account the actual location
  // bar height in the toolbar. Update this once that's been updated for the
  // refresh and remove |kLocationBarHeight|.
  CGFloat kLocationBarHeight = 38;
  CGFloat minHeightDiff =
      kLocationBarHeight - content_suggestions::kSearchFieldHeight;
  self.backgroundHeightConstraint.constant =
      content_suggestions::kSearchFieldHeight + minHeightDiff * percent;

  // TODO(crbug.com/805645) This should take into account the actual label width
  // of the toolbar location omnibox box hint text. Update this once that's been
  // updated for the refresh and remove |kHintShrinkWidth|.
  CGFloat kHintShrinkWidth = 30;
  CGFloat hintWidth =
      [hintLabel.text
          cr_boundingSizeWithSize:CGSizeMake(widthConstraint.constant, INFINITY)
                             font:hintLabel.font]
          .width;
  hintLabelWidthConstraint.constant = hintWidth - kHintShrinkWidth * percent;
  hintLabelWidthConstraint.active = YES;

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat constantDiff =
      percent * (ntp_header::kMaxHorizontalMarginDiff + safeAreaInsets.left);
  for (NSLayoutConstraint* constraint in constraints) {
    if (constraint.constant > 0)
      constraint.constant = constantDiff + ntp_header::kHintLabelSidePadding;
    else
      constraint.constant = -constantDiff;
  }
}

- (void)fadeOutShadow {
  // No-op.
}

- (void)hideToolbarViewsForNewTabPage {
  // No-op.
}

- (void)setToolbarTabCount:(int)tabCount {
  // No-op.
}

- (void)setCanGoForward:(BOOL)canGoForward {
  // No-op.
}

- (void)setCanGoBack:(BOOL)canGoBack {
  // No-op.
}

#pragma mark - ToolbarOwner

- (CGRect)toolbarFrame {
  return self.toolBarView.frame;
}

- (id<ToolbarSnapshotProviding>)toolbarSnapshotProvider {
  return self;
}

#pragma mark - ToolbarSnapshotProviding

- (UIView*)snapshotForTabSwitcher {
  return nil;
}

- (UIView*)snapshotForStackViewWithWidth:(CGFloat)width
                          safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  return self.toolBarView;
}

- (UIColor*)toolbarBackgroundColor {
  return nil;
}

@end
