// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_toolbar_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageHeaderView () {
  NewTabPageToolbarController* _toolbarController;
  UIImageView* _searchBoxBorder;
  UIImageView* _shadow;

  // Constraint specifying the height of the toolbar. Used to update the height
  // of the toolbar if the safe area changes.
  __weak NSLayoutConstraint* toolbarHeightConstraint_;
}

@end

@implementation NewTabPageHeaderView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.clipsToBounds = YES;
  }
  return self;
}


- (UIView*)toolBarView {
  return [_toolbarController view];
}

- (ToolbarController*)relinquishedToolbarController {
  ToolbarController* relinquishedToolbarController = nil;
  if ([[_toolbarController view] isDescendantOfView:self]) {
    // Only relinquish the toolbar controller if it's in the hierarchy.
    relinquishedToolbarController = _toolbarController;
  }
  return relinquishedToolbarController;
}

- (void)addConstraintsToToolbar {
  toolbarHeightConstraint_ = [[_toolbarController view].heightAnchor
      constraintEqualToConstant:
          [_toolbarController

              preferredToolbarHeightWhenAlignedToTopOfScreen]];

  [NSLayoutConstraint activateConstraints:@[
    [[_toolbarController view].leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [[_toolbarController view].topAnchor
        constraintEqualToAnchor:self.topAnchor],
    [[_toolbarController view].trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    toolbarHeightConstraint_
  ]];
}

- (void)reparentToolbarController {
  DCHECK(![[_toolbarController view] isDescendantOfView:self]);
  [self addSubview:[_toolbarController view]];
  if (base::FeatureList::IsEnabled(kSafeAreaCompatibleToolbar)) {
    [self addConstraintsToToolbar];
  }
}

- (void)addToolbarWithReadingListModel:(ReadingListModel*)readingListModel
                            dispatcher:(id)dispatcher {
  DCHECK(!_toolbarController);
  DCHECK(readingListModel);

  _toolbarController =
      [[NewTabPageToolbarController alloc] initWithDispatcher:dispatcher];
  _toolbarController.readingListModel = readingListModel;

  UIView* toolbarView = [_toolbarController view];

  [self addSubview:[_toolbarController view]];

  if (base::FeatureList::IsEnabled(kSafeAreaCompatibleToolbar)) {
    [self addConstraintsToToolbar];
  } else {
    CGRect toolbarFrame = self.bounds;
    toolbarFrame.size.height = ntp_header::kToolbarHeight;
    toolbarView.frame = toolbarFrame;
    [toolbarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  }
}

- (void)setCanGoForward:(BOOL)canGoForward {
  [_toolbarController setCanGoForward:canGoForward];
  [self hideToolbarViewsForNewTabPage];
}

- (void)setCanGoBack:(BOOL)canGoBack {
  [_toolbarController setCanGoBack:canGoBack];
  [self hideToolbarViewsForNewTabPage];
}

- (void)hideToolbarViewsForNewTabPage {
  [_toolbarController hideViewsForNewTabPage:YES];
};

- (void)setToolbarTabCount:(int)tabCount {
  [_toolbarController setTabCount:tabCount];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  [searchField setBackgroundColor:[UIColor whiteColor]];
  UIImage* searchBorderImage =
      StretchableImageNamed(@"ntp_google_search_box", 12, 12);
  _searchBoxBorder = [[UIImageView alloc] initWithImage:searchBorderImage];
  [_searchBoxBorder setFrame:[searchField bounds]];
  [_searchBoxBorder setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleHeight];
  [searchField insertSubview:_searchBoxBorder atIndex:0];

  UIImage* fullBleedShadow = NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED);
  _shadow = [[UIImageView alloc] initWithImage:fullBleedShadow];
  CGRect shadowFrame = [searchField bounds];
  shadowFrame.origin.y = searchField.bounds.size.height;
  shadowFrame.size.height = fullBleedShadow.size.height;
  [_shadow setFrame:shadowFrame];
  [_shadow setUserInteractionEnabled:NO];
  [_shadow setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                               UIViewAutoresizingFlexibleTopMargin];
  [searchField addSubview:_shadow];
  [_shadow setAlpha:0];
}

- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
            subviewConstraints:(NSArray*)constraints
                 logoIsShowing:(BOOL)logoIsShowing
                     forOffset:(CGFloat)offset
                         width:(CGFloat)width {
  CGFloat screenWidth = width > 0 ? width : self.bounds.size.width;
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

  if (screenWidth == 0)
    return;

  CGFloat searchFieldNormalWidth =
      content_suggestions::searchFieldWidth(screenWidth);

  // Calculate the amount to grow the width and height of searchField so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset = ui::AlignValueToUpperPixel(
      (searchFieldNormalWidth - screenWidth) / 2 - 1);
  CGFloat maxHeightDiff =
      ntp_header::kToolbarHeight - content_suggestions::kSearchFieldHeight;

  widthConstraint.constant = searchFieldNormalWidth - 2 * maxXInset * percent;
  topMarginConstraint.constant = content_suggestions::searchFieldTopMargin() +
                                 ntp_header::kMaxTopMarginDiff * percent;
  heightConstraint.constant =
      content_suggestions::kSearchFieldHeight + maxHeightDiff * percent;

  [_searchBoxBorder setAlpha:(1 - percent)];
  [_shadow setAlpha:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat constantDiff = percent * ntp_header::kMaxHorizontalMarginDiff;
  for (NSLayoutConstraint* constraint in constraints) {
    if (constraint.constant > 0)
      constraint.constant = constantDiff + ntp_header::kHintLabelSidePadding;
    else
      constraint.constant = -constantDiff;
  }
}

- (void)safeAreaInsetsDidChange {
  if (base::FeatureList::IsEnabled(kSafeAreaCompatibleToolbar)) {
    toolbarHeightConstraint_.constant =
        [_toolbarController preferredToolbarHeightWhenAlignedToTopOfScreen];
  }
}

- (void)fadeOutShadow {
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     [_shadow setAlpha:0];
                   }];
}

@end
