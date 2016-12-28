// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_toolbar_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

const CGFloat kOmniboxImageBottomInset = 1;
const CGFloat kHintLabelSidePadding = 12;
const CGFloat kMaxConstraintConstantDiff = 5;

}  // namespace

@interface NewTabPageHeaderView ()<TabModelObserver> {
  base::scoped_nsobject<NewTabPageToolbarController> _toolbarController;
  base::scoped_nsobject<TabModel> _tabModel;
  base::scoped_nsobject<UIImageView> _searchBoxBorder;
  base::scoped_nsobject<UIImageView> _shadow;
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

- (void)dealloc {
  [_tabModel removeObserver:self];
  [super dealloc];
}

- (UIView*)toolBarView {
  return [_toolbarController view];
}

- (ToolbarController*)relinquishedToolbarController {
  ToolbarController* relinquishedToolbarController = nil;
  if ([[_toolbarController view] isDescendantOfView:self]) {
    // Only relinquish the toolbar controller if it's in the hierarchy.
    relinquishedToolbarController = _toolbarController.get();
  }
  return relinquishedToolbarController;
}

- (void)reparentToolbarController {
  [self addSubview:[_toolbarController view]];
}

- (void)addToolbarWithDelegate:(id<WebToolbarDelegate>)toolbarDelegate
                       focuser:(id<OmniboxFocuser>)focuser
                      tabModel:(TabModel*)tabModel
              readingListModel:(ReadingListModel*)readingListModel {
  DCHECK(!_toolbarController);
  DCHECK(focuser);

  _toolbarController.reset([[NewTabPageToolbarController alloc]
      initWithToolbarDelegate:toolbarDelegate
                      focuser:focuser]);
  _toolbarController.get().readingListModel = readingListModel;
  [_tabModel removeObserver:self];
  _tabModel.reset([tabModel retain]);
  [self addTabModelObserver];

  UIView* toolbarView = [_toolbarController view];
  CGRect toolbarFrame = self.bounds;
  toolbarFrame.size.height = ntp_header::kToolbarHeight;
  toolbarView.frame = toolbarFrame;
  [toolbarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [self hideToolbarViewsForNewTabPage];

  [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [self addSubview:[_toolbarController view]];
}

- (void)hideToolbarViewsForNewTabPage {
  [_toolbarController hideViewsForNewTabPage:YES];
};

- (void)addTabModelObserver {
  [_tabModel addObserver:self];
  [_toolbarController setTabCount:[_tabModel count]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  [searchField setBackgroundColor:[UIColor whiteColor]];
  UIImage* searchBorderImage =
      StretchableImageNamed(@"ntp_google_search_box", 12, 12);
  _searchBoxBorder.reset([[UIImageView alloc] initWithImage:searchBorderImage]);
  [_searchBoxBorder setFrame:[searchField bounds]];
  [_searchBoxBorder setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleHeight];
  [searchField insertSubview:_searchBoxBorder atIndex:0];

  UIImage* fullBleedShadow = NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED);
  _shadow.reset([[UIImageView alloc] initWithImage:fullBleedShadow]);
  CGRect shadowFrame = [searchField bounds];
  shadowFrame.origin.y =
      searchField.bounds.size.height - kOmniboxImageBottomInset;
  shadowFrame.size.height = fullBleedShadow.size.height;
  [_shadow setFrame:shadowFrame];
  [_shadow setUserInteractionEnabled:NO];
  [_shadow setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                               UIViewAutoresizingFlexibleTopMargin];
  [searchField addSubview:_shadow];
  [_shadow setAlpha:0];
}

- (void)tabModelDidChangeTabCount:(TabModel*)model {
  DCHECK(model == _tabModel);
  [_toolbarController setTabCount:[_tabModel count]];
}

- (void)updateSearchField:(UIView*)searchField
         withInitialFrame:(CGRect)initialFrame
       subviewConstraints:(NSArray*)constraints
                forOffset:(CGFloat)offset {
  // The scroll offset at which point |searchField|'s frame should stop growing.
  CGFloat maxScaleOffset =
      self.frame.size.height - ntp_header::kMinHeaderHeight;
  // The scroll offset at which point |searchField|'s frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = MIN(1, MAX(0, animatingOffset / ntp_header::kAnimationDistance));
  }

  // Calculate the amount to grow the width and height of |searchField| so that
  // its frame covers the entire toolbar area.
  CGFloat maxXInset = ui::AlignValueToUpperPixel(
      (initialFrame.size.width - self.bounds.size.width) / 2 - 1);
  CGFloat maxYOffset = ui::AlignValueToUpperPixel(
      (ntp_header::kToolbarHeight - initialFrame.size.height) / 2 +
      kOmniboxImageBottomInset - 0.5);
  CGRect searchFieldFrame = CGRectInset(initialFrame, maxXInset * percent, 0);
  searchFieldFrame.origin.y += maxYOffset * percent;
  searchFieldFrame.size.height += 2 * maxYOffset * percent;
  [searchField setFrame:CGRectIntegral(searchFieldFrame)];
  [_searchBoxBorder setAlpha:(1 - percent)];
  [_shadow setAlpha:percent];

  // Adjust the position of the search field's subviews by adjusting their
  // constraint constant value.
  CGFloat constantDiff = percent * kMaxConstraintConstantDiff;
  for (NSLayoutConstraint* constraint in constraints) {
    if (constraint.constant > 0)
      constraint.constant = constantDiff + kHintLabelSidePadding;
    else
      constraint.constant = -constantDiff;
  }
}

- (void)fadeOutShadow {
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     [_shadow setAlpha:0];
                   }];
}

@end
