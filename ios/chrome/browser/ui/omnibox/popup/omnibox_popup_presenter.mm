// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kExpandAnimationDuration = 0.1;
const CGFloat kCollapseAnimationDuration = 0.05;
const CGFloat kShadowHeight = 10;
const CGFloat kiPadVerticalOffset = 3;
const CGFloat kRefreshVerticalOffset = 6;
NS_INLINE CGFloat BottomPadding() {
  return IsIPadIdiom() ? kShadowHeight : 0;
}
}  // namespace

@interface OmniboxPopupPresenter ()
// Constraint for the bottom anchor of the popup.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraint;

@property(nonatomic, weak) id<OmniboxPopupPositioner> positioner;
@property(nonatomic, weak) UIViewController* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
@end

@implementation OmniboxPopupPresenter
@synthesize viewController = _viewController;
@synthesize positioner = _positioner;
@synthesize popupContainerView = _popupContainerView;
@synthesize bottomConstraint = _bottomConstraint;

- (instancetype)initWithPopupPositioner:(id<OmniboxPopupPositioner>)positioner
                    popupViewController:(UIViewController*)viewController {
  self = [super init];
  if (self) {
    _positioner = positioner;
    _viewController = viewController;

    // Set up a container for presentation.
    UIView* popupContainer = [[UIView alloc] init];
    _popupContainerView = popupContainer;
    popupContainer.translatesAutoresizingMaskIntoConstraints = NO;
    popupContainer.layoutMargins = UIEdgeInsetsMake(0, 0, BottomPadding(), 0);

    // Add the view controller's view to the container.
    [popupContainer addSubview:viewController.view];
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraintsToSidesWithInsets(
        viewController.view, popupContainer,
        LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom |
            LayoutSides::kTop,
        ChromeDirectionalEdgeInsetsMake(0, 0, BottomPadding(), 0));

    // Add a shadow.
    UIImageView* shadowView = [[UIImageView alloc]
        initWithImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED)];
    [shadowView setUserInteractionEnabled:NO];
    [shadowView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [popupContainer addSubview:shadowView];

    // On iPhone, the shadow is on the top of the popup, as if it's cast by
    // the omnibox; on iPad, the shadow is cast by the popup instead, so it's
    // below the popup.
    AddSameConstraintsToSides(shadowView, popupContainer,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    AddSameConstraintsToSides(
        shadowView, popupContainer,
        IsIPadIdiom() ? LayoutSides::kBottom : LayoutSides::kTop);
  }
  return self;
}

- (void)updateHeightAndAnimateAppearanceIfNecessary {
  UIView* popup = self.popupContainerView;
  if (!popup.superview) {
    [[self.positioner popupParentView] addSubview:popup];
    [self initialLayout];
  }

  if (!IsIPadIdiom()) {
    self.bottomConstraint.active = YES;
  }

  if (popup.bounds.size.height == 0) {
    // Animate if it expanding.
    [UIView animateWithDuration:kExpandAnimationDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       [[popup superview] layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)animateCollapse {
  UIView* retainedPopupView = self.popupContainerView;
  if (!IsIPadIdiom()) {
    self.bottomConstraint.active = NO;
  }

  [UIView animateWithDuration:kCollapseAnimationDuration
      delay:0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [[self.popupContainerView superview] layoutIfNeeded];
      }
      completion:^(BOOL) {
        [retainedPopupView removeFromSuperview];
      }];
}

#pragma mark - Private

// Layouts the popup when it is just added to the view hierarchy.
- (void)initialLayout {
  UIView* popup = self.popupContainerView;
  // Creates the constraints if the view is newly added to the view hierarchy.
  // On iPad the height of the popup is fixed.

  // This constraint will only be activated on iPhone as the popup is taking
  // the full height.
  self.bottomConstraint = [popup.bottomAnchor
      constraintEqualToAnchor:[popup superview].bottomAnchor];

  // Position the top anchor of the popup relatively to the layout guide
  // positioned on the omnibox.
  UILayoutGuide* topLayout =
      [NamedGuide guideWithName:kOmniboxGuide view:popup];
  NSLayoutConstraint* topConstraint =
      [popup.topAnchor constraintEqualToAnchor:topLayout.bottomAnchor];
  if (IsUIRefreshPhase1Enabled()) {
    // TODO(crbug.com/846337) Remove this workaround and clean up popup
    // presentation.
    topConstraint.constant = kRefreshVerticalOffset;
  } else if (IsIPadIdiom()) {
    topConstraint.constant = kiPadVerticalOffset;
  }

  [NSLayoutConstraint activateConstraints:@[
    [popup.leadingAnchor constraintEqualToAnchor:popup.superview.leadingAnchor],
    [popup.trailingAnchor
        constraintEqualToAnchor:popup.superview.trailingAnchor],
    topConstraint,
  ]];

  [popup layoutIfNeeded];
  [[popup superview] layoutIfNeeded];
}

@end
