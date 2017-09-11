// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar.h"

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar_commands.h"
#import "ios/clean/chrome/browser/ui/tab_grid/ui_button+cr_tab_grid.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolbarHeight = 44.0f;
const CGFloat kSpacing = 16.0f;
}

@interface TabGridToolbar ()
@property(nonatomic, weak) UIStackView* toolbarContent;
@property(nonatomic, weak) UIVisualEffectView* toolbarView;
@end

@implementation TabGridToolbar

@synthesize toolbarContent = _toolbarContent;
@synthesize dispatcher = _dispatcher;
@synthesize toolbarView = _toolbarView;

- (instancetype)init {
  if (self = [super init]) {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* toolbarView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    toolbarView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:toolbarView];

    UIStackView* toolbarContent = [self content];
    [toolbarView.contentView addSubview:toolbarContent];
    _toolbarView = toolbarView;
    _toolbarContent = toolbarContent;
    // Sets the stackview to a fixed height, anchored to the bottom of the
    // blur view.
    _toolbarContent.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_toolbarContent.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [_toolbarContent.leadingAnchor
          constraintEqualToAnchor:toolbarView.contentView.leadingAnchor],
      [_toolbarContent.trailingAnchor
          constraintEqualToAnchor:toolbarView.contentView.trailingAnchor],
      [_toolbarContent.bottomAnchor
          constraintEqualToAnchor:toolbarView.contentView.bottomAnchor]
    ]];
  }
  return self;
}

- (void)setIncognito:(BOOL)incognito {
  // TODO: Add correct colors.
  if (incognito) {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
    self.toolbarView.effect = blurEffect;
  } else {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    self.toolbarView.effect = blurEffect;
  }
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  UIView* toolsMenuButtonView =
      [self.toolbarContent.arrangedSubviews lastObject];
  return [view convertRect:toolsMenuButtonView.bounds
                  fromView:toolsMenuButtonView];
}

#pragma mark - UIView

// Returns an intrinsic height so that explicit height constraints are
// not necessary. Status bar conditional logic can be set using this
// intrinsic size.
- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kToolbarHeight);
}

#pragma mark - Toolbar actions

- (void)showToolsMenu {
  [self.dispatcher showToolsMenu];
}

- (void)toggleIncognito {
  [self.dispatcher toggleIncognito];
}

#pragma mark - Private

// Returns a stack view containing the elements of the toolbar.
- (UIStackView*)content {
  UIView* spacerView = [[UIView alloc] init];
  [spacerView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                forAxis:UILayoutConstraintAxisHorizontal];
  UIButton* menuButton = [UIButton cr_tabGridToolbarMenuButton];
  [menuButton addTarget:self
                 action:@selector(showToolsMenu)
       forControlEvents:UIControlEventTouchUpInside];

  UIButton* incognitoButton = [UIButton cr_tabGridToolbarIncognitoButton];
  [incognitoButton addTarget:self
                      action:@selector(toggleIncognito)
            forControlEvents:UIControlEventTouchUpInside];

  NSArray* items = @[
    [UIButton cr_tabGridToolbarDoneButton], incognitoButton, spacerView,
    menuButton
  ];
  UIStackView* content = [[UIStackView alloc] initWithArrangedSubviews:items];
  content.layoutMarginsRelativeArrangement = YES;
  content.layoutMargins = UIEdgeInsetsMakeDirected(0, kSpacing, 0, kSpacing);
  content.spacing = kSpacing;
  content.distribution = UIStackViewDistributionFill;
  return content;
}

@end
