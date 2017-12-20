// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_presenter.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kExpandAnimationDuration = 0.1;
const CGFloat kCollapseAnimationDuration = 0.05;
const CGFloat kWhiteBackgroundHeight = 74;
NS_INLINE CGFloat ShadowHeight() {
  return IsIPadIdiom() ? 10 : 0;
}
}  // namespace

@interface OmniboxPopupPresenter ()
// Constraint for the height of the popup.
@property(nonatomic, strong) NSLayoutConstraint* heightConstraint;
// Constraint for the bottom anchor of the popup.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraint;

@property(nonatomic, weak) id<OmniboxPopupPositioner> positioner;
@property(nonatomic, weak) UITableViewController* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
@end

@implementation OmniboxPopupPresenter
@synthesize viewController = _viewController;
@synthesize positioner = _positioner;
@synthesize popupContainerView = _popupContainerView;
@synthesize heightConstraint = _heightConstraint;
@synthesize bottomConstraint = _bottomConstraint;

- (instancetype)initWithPopupPositioner:(id<OmniboxPopupPositioner>)positioner
                    popupViewController:(UITableViewController*)viewController {
  self = [super init];
  if (self) {
    _positioner = positioner;
    _viewController = viewController;

    if (IsIPadIdiom()) {
      _popupContainerView = [OmniboxPopupPresenter newBackgroundViewIpad];
    } else {
      _popupContainerView = [OmniboxPopupPresenter newBackgroundViewIPhone];
    }
    _popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    _heightConstraint =
        [_popupContainerView.heightAnchor constraintEqualToConstant:0];
    _heightConstraint.active = YES;

    CGRect popupControllerFrame = viewController.view.frame;
    popupControllerFrame.origin = CGPointZero;
    viewController.view.frame = popupControllerFrame;
    [_popupContainerView addSubview:viewController.view];
  }
  return self;
}

- (void)updateHeightAndAnimateAppearanceIfNecessary {
  UIView* popup = self.popupContainerView;
  UIView* siblingView = [self.positioner popupAnchorView];
  BOOL newlyAdded = ([popup superview] == nil);

  if (IsIPadIdiom()) {
    [[siblingView superview] insertSubview:popup aboveSubview:siblingView];
  } else {
    [[siblingView superview] insertSubview:popup belowSubview:siblingView];
  }

  if (newlyAdded) {
    [self initialLayout];
  }

  CGFloat currentHeight = popup.bounds.size.height;
  if (IsIPadIdiom()) {
    // Show |result.size| on iPad.
    CGFloat height = [[self.viewController tableView] contentSize].height;
    UIEdgeInsets insets = [[self.viewController tableView] contentInset];
    // Note the calculation |insets.top * 2| is correct, it should not be
    // insets.top + insets.bottom. |insets.bottom| will be larger than
    // |insets.top| when the keyboard is visible, but |parentHeight| should stay
    // the same.
    CGFloat iPadHeight = height + insets.top * 2 + ShadowHeight();
    self.heightConstraint.constant = iPadHeight;
  } else {
    self.heightConstraint.active = NO;
    self.bottomConstraint.active = YES;
  }

  if (currentHeight == 0) {
    // Animate if it expanding.
    [UIView animateWithDuration:kExpandAnimationDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       [[popup superview] layoutIfNeeded];
                     }
                     completion:nil];
  }

  // Set the size the table view.
  CGRect popupControllerFrame = self.viewController.view.frame;
  popupControllerFrame.size.height = popup.frame.size.height - ShadowHeight();
  self.viewController.view.frame = popupControllerFrame;
}

- (void)animateCollapse {
  UIView* retainedPopupView = self.popupContainerView;
  self.heightConstraint.constant = 0;
  if (!IsIPadIdiom()) {
    self.bottomConstraint.active = NO;
    self.heightConstraint.active = YES;
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
  self.heightConstraint.constant = 0;

  // This constraint will only be activated on iPhone as the popup is taking
  // the full height.
  self.bottomConstraint = [popup.bottomAnchor
      constraintEqualToAnchor:[popup superview].bottomAnchor];

  // Position the top anchor of the popup relatively to the layout guide
  // positioned on the omnibox.
  UILayoutGuide* topLayout = FindNamedGuide(kOmniboxGuide, popup);
  NSLayoutConstraint* topConstraint =
      [popup.topAnchor constraintEqualToAnchor:topLayout.bottomAnchor];
  if (IsIPadIdiom()) {
    if (base::FeatureList::IsEnabled(kCleanToolbar)) {
      topConstraint.constant = kiPadOmniboxPopupVerticalOffset;
    } else {
      // On the old toolbar, the layout guide is positioned on the toolbar
      // instead of on the omnibox. The popup is positioned between the bottom
      // of the toolbar and the bottom of the omnibox. Adding the opposite of
      // the constant positions it correctly.
      topConstraint.constant = -kiPadOmniboxPopupVerticalOffset;
    }
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

#pragma mark - Background creation

+ (UIView*)newBackgroundViewIpad {
  UIView* view = [[UIView alloc] init];
  [view setClipsToBounds:YES];

  UIImageView* shadowView = [[UIImageView alloc]
      initWithImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED)];
  [shadowView setUserInteractionEnabled:NO];
  [shadowView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [view addSubview:shadowView];

  // Add constraints to position |shadowView| at the bottom of |view|
  // with the same width as |view|.
  NSDictionary* views = NSDictionaryOfVariableBindings(shadowView);
  [view addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"H:|[shadowView]|"
                                               options:0
                                               metrics:nil
                                                 views:views]];
  [view addConstraint:[NSLayoutConstraint
                          constraintWithItem:shadowView
                                   attribute:NSLayoutAttributeBottom
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:view
                                   attribute:NSLayoutAttributeBottom
                                  multiplier:1
                                    constant:0]];

  return view;
}

+ (UIView*)newBackgroundViewIPhone {
  UIView* view = [[UIView alloc] init];

  // Add a white background to prevent seeing the logo scroll through the
  // omnibox.
  UIView* whiteBackground = [[UIView alloc] initWithFrame:CGRectZero];
  [view addSubview:whiteBackground];
  [whiteBackground setBackgroundColor:[UIColor whiteColor]];

  // Set constraints to |whiteBackground|.
  [whiteBackground setTranslatesAutoresizingMaskIntoConstraints:NO];
  NSDictionary* metrics = @{ @"height" : @(kWhiteBackgroundHeight) };
  NSDictionary* views = NSDictionaryOfVariableBindings(whiteBackground);
  [view addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"H:|[whiteBackground]|"
                                               options:0
                                               metrics:nil
                                                 views:views]];
  [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:
                                               @"V:[whiteBackground(==height)]"
                                                               options:0
                                                               metrics:metrics
                                                                 views:views]];
  [view addConstraint:[NSLayoutConstraint
                          constraintWithItem:whiteBackground
                                   attribute:NSLayoutAttributeBottom
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:view
                                   attribute:NSLayoutAttributeTop
                                  multiplier:1
                                    constant:0]];
  return view;
}

@end
