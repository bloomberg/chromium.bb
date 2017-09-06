// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_presenter.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
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

@implementation OmniboxPopupPresenter
@synthesize viewController = _viewController;
@synthesize positioner = _positioner;
@synthesize popupContainerView = _popupContainerView;

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

    CGRect popupControllerFrame = viewController.view.frame;
    popupControllerFrame.origin = CGPointZero;
    viewController.view.frame = popupControllerFrame;
    [_popupContainerView addSubview:viewController.view];
  }
  return self;
}

- (void)updateHeightAndAnimateAppearanceIfNecessary {
  UIView* view = self.popupContainerView;
  // Show |result.size| on iPad.  Since iPhone can dismiss keyboard, set
  // height to frame height.
  CGFloat height = [[self.viewController tableView] contentSize].height;
  UIEdgeInsets insets = [[self.viewController tableView] contentInset];
  // Note the calculation |insets.top * 2| is correct, it should not be
  // insets.top + insets.bottom. |insets.bottom| will be larger than
  // |insets.top| when the keyboard is visible, but |parentHeight| should stay
  // the same.
  CGFloat parentHeight = height + insets.top * 2 + ShadowHeight();
  UIView* siblingView = [self.positioner popupAnchorView];
  if (IsIPadIdiom()) {
    [[siblingView superview] insertSubview:view aboveSubview:siblingView];
  } else {
    [view setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleHeight];
    [[siblingView superview] insertSubview:view belowSubview:siblingView];
  }

  CGFloat currentHeight = view.layer.bounds.size.height;
  if (currentHeight == 0)
    [self animateAppearance:parentHeight];
  else
    [view setFrame:[self.positioner popupFrame:parentHeight]];

  CGRect popupControllerFrame = self.viewController.view.frame;
  popupControllerFrame.size.height = view.frame.size.height - ShadowHeight();
  self.viewController.view.frame = popupControllerFrame;
}

- (void)animateAppearance:(CGFloat)parentHeight {
  CGRect popupFrame = [self.positioner popupFrame:parentHeight];
  CALayer* popupLayer = self.popupContainerView.layer;
  CGRect bounds = popupLayer.bounds;
  bounds.size.height = popupFrame.size.height;
  popupLayer.bounds = bounds;

  CGRect frame = self.popupContainerView.frame;
  frame.size.width = popupFrame.size.width;
  frame.origin.y = popupFrame.origin.y;
  self.popupContainerView.frame = frame;

  CABasicAnimation* growHeight =
      [CABasicAnimation animationWithKeyPath:@"bounds.size.height"];
  growHeight.fromValue = @0;
  growHeight.toValue = [NSNumber numberWithFloat:popupFrame.size.height];
  growHeight.duration = kExpandAnimationDuration;
  growHeight.timingFunction =
      [CAMediaTimingFunction functionWithControlPoints:0.4:0:0.2:1];
  [popupLayer addAnimation:growHeight forKey:@"growHeight"];
}

- (void)animateCollapse {
  CALayer* popupLayer = self.popupContainerView.layer;
  CGRect bounds = popupLayer.bounds;
  CGFloat currentHeight = bounds.size.height;
  bounds.size.height = 0;
  popupLayer.bounds = bounds;

  UIView* retainedPopupView = self.popupContainerView;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [retainedPopupView removeFromSuperview];
  }];
  CABasicAnimation* shrinkHeight =
      [CABasicAnimation animationWithKeyPath:@"bounds.size.height"];
  shrinkHeight.fromValue = [NSNumber numberWithFloat:currentHeight];
  shrinkHeight.toValue = @0;
  shrinkHeight.duration = kCollapseAnimationDuration;
  shrinkHeight.timingFunction =
      [CAMediaTimingFunction functionWithControlPoints:0.4:0:1:1];
  [popupLayer addAnimation:shrinkHeight forKey:@"shrinkHeight"];
  [CATransaction commit];
}

+ (UIView*)newBackgroundViewIpad {
  UIView* view = [[UIView alloc] init];
  [view setClipsToBounds:YES];

  // Adjust popupView_'s anchor point and height so that it animates down
  // from the top when it appears.
  view.layer.anchorPoint = CGPointMake(0.5, 0);

  [view setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
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

  // Adjust popupView_'s anchor point and height so that it animates down
  // from the top when it appears.
  view.layer.anchorPoint = CGPointMake(0.5, 0);

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
