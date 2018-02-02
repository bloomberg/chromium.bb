// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsHeaderView ()<ToolbarSnapshotProviding>
@end

@implementation ContentSuggestionsHeaderView

@synthesize toolBarView = _toolBarView;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // TODO(crbug.com/808429) This logic is specific to phone.  This will need to
  // be updated when tablet also shows the |toolBarView|.
  self.toolBarView.hidden =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular;
}

#pragma mark - NTPHeaderViewAdapter

- (void)addToolbarView:(UIView*)toolbarView {
  DCHECK(!self.toolBarView);
  _toolBarView = toolbarView;
  [self addSubview:self.toolBarView];
  // TODO(crbug.com/808431) This logic is duplicated in various places and
  // should be refactored away for content suggestions.
  AddNamedGuide(kOmniboxGuide, self);
  AddNamedGuide(kBackButtonGuide, self);
  AddNamedGuide(kForwardButtonGuide, self);
  AddNamedGuide(kToolsMenuGuide, self);
  AddNamedGuide(kTabSwitcherGuide, self);
  [NSLayoutConstraint activateConstraints:@[
    [self.toolBarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.toolBarView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [self.toolBarView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  // TODO(crbug.com/805644) Update fake omnibox theme.
}

- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
            subviewConstraints:(NSArray*)constraints
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGFloat contentWidth = std::max<CGFloat>(
      0, screenWidth - safeAreaInsets.left - safeAreaInsets.right);
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset =
      self.frame.size.height - ntp_header::kMinHeaderHeight;
  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = MIN(1, MAX(0, animatingOffset / ntp_header::kAnimationDistance));
  }

  if (screenWidth == 0 || contentWidth == 0)
    return;

  CGFloat searchFieldNormalWidth =
      content_suggestions::searchFieldWidth(contentWidth);

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset = ui::AlignValueToUpperPixel(
      (searchFieldNormalWidth - screenWidth) / 2 - 1);
  CGFloat maxHeightDiff =
      ntp_header::kToolbarHeight - content_suggestions::kSearchFieldHeight;

  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::searchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant =
      content_suggestions::kSearchFieldHeight + maxHeightDiff * percent;

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
