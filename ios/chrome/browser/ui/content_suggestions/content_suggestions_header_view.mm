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
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Landscape inset for fake omnibox background container
const CGFloat kBackgroundLandscapeInset = 169;

// Fakebox highlight animation duration.
const CGFloat kFakeboxHighlightDuration = 0.4;

// Fakebox highlight background alpha increase.
const CGFloat kFakeboxHighlightIncrease = 0.06;

// Returns the height of the toolbar based on the preferred content size of the
// application.
CGFloat ToolbarHeight() {
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  return ToolbarExpandedHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
}

}  // namespace

@interface ContentSuggestionsHeaderView ()

@property(nonatomic, strong, readwrite) UIButton* voiceSearchButton;

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* fakeLocationBarTopConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarHeightConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarLeadingConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeToolbarTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* voiceSearchTrailingConstraint;
@property(nonatomic, strong) UIView* fakeLocationBar;
@property(nonatomic, strong) UILabel* searchHintLabel;

@end

@implementation ContentSuggestionsHeaderView

@synthesize fakeLocationBar = _fakeLocationBar;
@synthesize fakeLocationBarTopConstraint = _fakeLocationBarTopConstraint;
@synthesize fakeLocationBarHeightConstraint = _fakeLocationBarHeightConstraint;
@synthesize fakeLocationBarLeadingConstraint =
    _fakeLocationBarLeadingConstraint;
@synthesize fakeLocationBarTrailingConstraint =
    _fakeLocationBarTrailingConstraint;
@synthesize fakeToolbarTopConstraint = _fakeToolbarTopConstraint;
@synthesize voiceSearchTrailingConstraint = _voiceSearchTrailingConstraint;
@synthesize hintLabelLeadingConstraint = _hintLabelLeadingConstraint;
@synthesize toolBarView = _toolBarView;
@synthesize searchHintLabel = _searchHintLabel;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)addToolbarView:(UIView*)toolbarView {
  _toolBarView = toolbarView;
  [self addSubview:toolbarView];
  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbarView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
    [toolbarView.heightAnchor constraintEqualToConstant:ToolbarHeight()],
    [toolbarView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor]
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  // Fake Toolbar.
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:NORMAL];
  UIBlurEffect* blurEffect = buttonFactory.toolbarConfiguration.blurEffect;
  UIView* fakeToolbar = nil;
  UIView* fakeToolbarContentView;
  if (blurEffect) {
    UIVisualEffectView* visualEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    fakeToolbar = visualEffectView;
    fakeToolbarContentView = visualEffectView.contentView;
  } else {
    fakeToolbar = [[UIView alloc] init];
    fakeToolbarContentView = fakeToolbar;
  }
  fakeToolbar.backgroundColor =
      buttonFactory.toolbarConfiguration.blurBackgroundColor;
  [searchField insertSubview:fakeToolbar atIndex:0];
  fakeToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  // Fake location bar.
  [fakeToolbarContentView addSubview:self.fakeLocationBar];

  // Hint label.
  self.searchHintLabel = [[UILabel alloc] init];
  content_suggestions::configureSearchHintLabel(self.searchHintLabel,
                                                searchField);
  self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
      constraintGreaterThanOrEqualToAnchor:[searchField leadingAnchor]
                                  constant:ntp_header::kHintLabelSidePadding];
  [NSLayoutConstraint activateConstraints:@[
    self.hintLabelLeadingConstraint,
    [self.searchHintLabel.heightAnchor
        constraintEqualToAnchor:self.fakeLocationBar.heightAnchor
                       constant:-ntp_header::kHintLabelHeightMargin],
    [self.searchHintLabel.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
  ]];
  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  self.searchHintLabel.isAccessibilityElement = NO;

  // Voice search.
  self.voiceSearchButton = [[UIButton alloc] init];
  content_suggestions::configureVoiceSearchButton(self.voiceSearchButton,
                                                  searchField);

  // Constraints.
  self.fakeToolbarTopConstraint =
      [fakeToolbar.topAnchor constraintEqualToAnchor:searchField.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [fakeToolbar.leadingAnchor
        constraintEqualToAnchor:searchField.leadingAnchor],
    [fakeToolbar.trailingAnchor
        constraintEqualToAnchor:searchField.trailingAnchor],
    self.fakeToolbarTopConstraint,
    [fakeToolbar.bottomAnchor constraintEqualToAnchor:searchField.bottomAnchor]
  ]];

  self.fakeLocationBarTopConstraint = [self.fakeLocationBar.topAnchor
      constraintEqualToAnchor:searchField.topAnchor];
  self.fakeLocationBarLeadingConstraint = [self.fakeLocationBar.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.fakeLocationBarTrailingConstraint = [self.fakeLocationBar.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.fakeLocationBarHeightConstraint = [self.fakeLocationBar.heightAnchor
      constraintEqualToConstant:ToolbarHeight()];
  [NSLayoutConstraint activateConstraints:@[
    self.fakeLocationBarTopConstraint,
    self.fakeLocationBarLeadingConstraint,
    self.fakeLocationBarTrailingConstraint,
    self.fakeLocationBarHeightConstraint,
  ]];

  self.voiceSearchTrailingConstraint = [self.voiceSearchButton.trailingAnchor
      constraintEqualToAnchor:[searchField trailingAnchor]];
  [NSLayoutConstraint activateConstraints:@[
    [self.voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    [self.searchHintLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.voiceSearchButton.leadingAnchor],
    self.voiceSearchTrailingConstraint
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset
                         safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset = self.frame.size.height - ToolbarHeight() -
                           ntp_header::kFakeOmniboxScrolledToTopMargin -
                           safeAreaInsets.top;

  // With RxR the search field should scroll under the toolbar.
  if (IsRegularXRegularSizeClass(self)) {
    maxScaleOffset += ToolbarHeight();
  }

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
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGFloat contentWidth = std::max<CGFloat>(
      0, screenWidth - safeAreaInsets.left - safeAreaInsets.right);
  if (screenWidth == 0 || contentWidth == 0)
    return;

  CGFloat searchFieldNormalWidth =
      content_suggestions::searchFieldWidth(contentWidth);

  CGFloat percent =
      [self searchFieldProgressForOffset:offset safeAreaInsets:safeAreaInsets];
  if (!IsSplitToolbarMode(self)) {
    self.alpha = 1 - percent;
    widthConstraint.constant = searchFieldNormalWidth;
    self.fakeLocationBarHeightConstraint.constant = ToolbarHeight();
    self.fakeLocationBar.layer.cornerRadius =
        self.fakeLocationBarHeightConstraint.constant / 2;
    [self scaleHintLabelForPercent:percent];
    self.fakeToolbarTopConstraint.constant = 0;

    self.fakeLocationBarLeadingConstraint.constant = 0;
    self.fakeLocationBarTrailingConstraint.constant = 0;
    self.fakeLocationBarTopConstraint.constant = 0;

    return;
  } else {
    self.alpha = 1;
  }

  // Grow the blur to cover the safeArea top.
  self.fakeToolbarTopConstraint.constant = -safeAreaInsets.top * percent;

  CGFloat toolbarExpandedHeight = ToolbarHeight();

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset =
      ui::AlignValueToUpperPixel((searchFieldNormalWidth - screenWidth) / 2);
  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = -content_suggestions::searchFieldTopMargin() -
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant = toolbarExpandedHeight;

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  CGFloat inset = !IsSplitToolbarMode() ? kBackgroundLandscapeInset : 0;
  self.fakeLocationBarLeadingConstraint.constant =
      (safeAreaInsets.left + kExpandedLocationBarHorizontalMargin + inset) *
      percent;
  self.fakeLocationBarTrailingConstraint.constant =
      -(safeAreaInsets.right + kExpandedLocationBarHorizontalMargin + inset) *
      percent;

  self.fakeLocationBarTopConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint * percent;
  // Use UIApplication preferredContentSizeCategory as this VC has a weird trait
  // collection from times to times.
  CGFloat kLocationBarHeight = LocationBarHeight(
      [UIApplication sharedApplication].preferredContentSizeCategory);
  CGFloat minHeightDiff = kLocationBarHeight - toolbarExpandedHeight;
  self.fakeLocationBarHeightConstraint.constant =
      toolbarExpandedHeight + minHeightDiff * percent;
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  // Scale the hintLabel, and make sure the frame stays left aligned.
  [self scaleHintLabelForPercent:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat subviewsDiff = -maxXInset * percent;
  self.voiceSearchTrailingConstraint.constant = -subviewsDiff;
  self.hintLabelLeadingConstraint.constant =
      subviewsDiff + ntp_header::kHintLabelSidePadding;
}

- (void)setFakeboxHighlighted:(BOOL)highlighted {
  [UIView animateWithDuration:kFakeboxHighlightDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     CGFloat alpha = kAdaptiveLocationBarBackgroundAlpha;
                     if (highlighted)
                       alpha += kFakeboxHighlightIncrease;
                     self.fakeLocationBar.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

#pragma mark - Property accessors

- (UIView*)fakeLocationBar {
  if (!_fakeLocationBar) {
    _fakeLocationBar = [[UIView alloc] init];
    _fakeLocationBar.userInteractionEnabled = NO;
    _fakeLocationBar.backgroundColor =
        [UIColor colorWithWhite:0 alpha:kAdaptiveLocationBarBackgroundAlpha];
    _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _fakeLocationBar;
}

#pragma mark - Private

// Scale the the hint label down to at most content_suggestions::kHintTextScale.
- (void)scaleHintLabelForPercent:(CGFloat)percent {
  CGFloat scaleValue =
      1 + (content_suggestions::kHintTextScale * (1 - percent));
  self.searchHintLabel.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

@end
