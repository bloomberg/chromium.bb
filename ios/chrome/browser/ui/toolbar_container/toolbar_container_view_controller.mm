// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarContainerViewController ()
// The constraint managing the height of the container.
@property(nonatomic, strong, readonly) NSLayoutConstraint* heightConstraint;
// The height constraints for the toolbar views.
@property(nonatomic, strong, readonly)
    NSMutableArray<CollapsingToolbarHeightConstraint*>*
        toolbarHeightConstraints;
// Returns the height constraint for the first toolbar in self.toolbars.
@property(nonatomic, readonly)
    CollapsingToolbarHeightConstraint* firstToolbarHeightConstraint;
// Additional height to be added to the first toolbar in the stack.
@property(nonatomic, assign) CGFloat additionalStackHeight;
@end

@implementation ToolbarContainerViewController
@synthesize orientation = _orientation;
@synthesize collapsesSafeArea = _collapsesSafeArea;
@synthesize toolbars = _toolbars;
@synthesize heightConstraint = _heightConstraint;
@synthesize toolbarHeightConstraints = _toolbarHeightConstraints;
@synthesize additionalStackHeight = _additionalStackHeight;

#pragma mark - Accessors

- (CollapsingToolbarHeightConstraint*)firstToolbarHeightConstraint {
  if (!self.viewLoaded || !self.toolbars.count)
    return nil;
  DCHECK_EQ(self.toolbarHeightConstraints.count, self.toolbars.count);
  DCHECK_EQ(self.toolbarHeightConstraints[0].firstItem, self.toolbars[0].view);
  return self.toolbarHeightConstraints[0];
}

- (void)setAdditionalStackHeight:(CGFloat)additionalStackHeight {
  if (AreCGFloatsEqual(_additionalStackHeight, additionalStackHeight))
    return;
  _additionalStackHeight = additionalStackHeight;
  self.firstToolbarHeightConstraint.additionalHeight = _additionalStackHeight;
  [self updateHeightConstraint];
}

#pragma mark - Public

- (CGFloat)toolbarStackHeightForFullscreenProgress:(CGFloat)progress {
  CGFloat height = 0.0;
  for (CollapsingToolbarHeightConstraint* constraint in self
           .toolbarHeightConstraints) {
    height += [constraint toolbarHeightForProgress:progress];
  }
  return height;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  for (CollapsingToolbarHeightConstraint* heightConstraint in self
           .toolbarHeightConstraints) {
    heightConstraint.progress = progress;
  }
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  __weak ToolbarContainerViewController* weakSelf = self;
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [weakSelf updateForFullscreenProgress:finalProgress];
    [[weakSelf view] setNeedsLayout];
    [[weakSelf view] layoutIfNeeded];
  }];
}

#pragma mark - ToolbarContainerConsumer

- (void)setOrientation:(ToolbarContainerOrientation)orientation {
  if (_orientation == orientation)
    return;
  _orientation = orientation;
  [self setUpToolbarStack];
}

- (void)setCollapsesSafeArea:(BOOL)collapsesSafeArea {
  if (_collapsesSafeArea == collapsesSafeArea)
    return;
  _collapsesSafeArea = collapsesSafeArea;
  self.firstToolbarHeightConstraint.collapsesAdditionalHeight = YES;
}

- (void)setToolbars:(NSArray<UIViewController*>*)toolbars {
  if ([_toolbars isEqualToArray:toolbars])
    return;
  [self removeToolbars];
  _toolbars = toolbars;
  [self setUpToolbarStack];
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[ToolbarContainerView alloc] init];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  _heightConstraint = [self.view.heightAnchor constraintEqualToConstant:0.0];
  _heightConstraint.active = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self setUpToolbarStack];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self removeToolbars];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateForSafeArea];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateForSafeArea];
}

#pragma mark - Layout Helpers

// Sets up the stack of toolbars.
- (void)setUpToolbarStack {
  if (!self.viewLoaded)
    return;
  [self removeToolbars];
  for (NSUInteger i = 0; i < self.toolbars.count; ++i) {
    [self addToolbarAtIndex:i];
  }
  [self createToolbarHeightConstraints];
  [self updateForSafeArea];
  [self updateHeightConstraint];
}

// Removes all the toolbars from the view.
- (void)removeToolbars {
  for (UIViewController* toolbar in self.toolbars) {
    [toolbar willMoveToParentViewController:nil];
    [toolbar.view removeFromSuperview];
    [toolbar removeFromParentViewController];
  }
  [self resetToolbarHeightConstraints];
}

// Adds the toolbar at |index| to the view.
- (void)addToolbarAtIndex:(NSUInteger)index {
  DCHECK_LT(index, self.toolbars.count);
  UIViewController* toolbar = self.toolbars[index];
  if (toolbar.parentViewController == self)
    return;

  // Add the toolbar and its view controller.
  UIView* toolbarView = toolbar.view;
  [self addChildViewController:toolbar];
  [self.view addSubview:toolbar.view];
  toolbarView.translatesAutoresizingMaskIntoConstraints = NO;
  [toolbar didMoveToParentViewController:self];

  // The toolbars will always be the full width of the container.
  AddSameConstraintsToSides(self.view, toolbarView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  // Calculate the positioning constraint.
  BOOL topToBottom =
      self.orientation == ToolbarContainerOrientation::kTopToBottom;
  NSLayoutAnchor* toolbarPositioningAnchor =
      topToBottom ? toolbarView.topAnchor : toolbarView.bottomAnchor;
  NSLayoutAnchor* positioningAnchor = nil;
  if (index > 0) {
    NSUInteger previousIndex = index - 1;
    UIViewController* previousToolbar = self.toolbars[previousIndex];
    DCHECK_EQ(previousToolbar.parentViewController, self);
    UIView* previousToolbarView = previousToolbar.view;
    positioningAnchor = topToBottom ? previousToolbarView.bottomAnchor
                                    : previousToolbarView.topAnchor;
  } else {
    positioningAnchor =
        topToBottom ? self.view.topAnchor : self.view.bottomAnchor;
  }
  [toolbarPositioningAnchor constraintEqualToAnchor:positioningAnchor].active =
      YES;
}

// Deactivates the toolbar height constraints and resets the property.
- (void)resetToolbarHeightConstraints {
  if (_toolbarHeightConstraints.count)
    [NSLayoutConstraint deactivateConstraints:_toolbarHeightConstraints];
  _toolbarHeightConstraints = nil;
}

// Creates and activates height constriants for the toolbars and adds them to
// self.toolbarHeightConstraints at the same index of their corresponding
// toolbar view controller.
- (void)createToolbarHeightConstraints {
  [self resetToolbarHeightConstraints];
  _toolbarHeightConstraints = [NSMutableArray array];
  for (NSUInteger i = 0; i < self.toolbars.count; ++i) {
    UIView* toolbarView = self.toolbars[i].view;
    CollapsingToolbarHeightConstraint* heightConstraint =
        [CollapsingToolbarHeightConstraint constraintWithView:toolbarView];
    heightConstraint.active = YES;
    // Set up the additional height for the first toolbar.
    if (!self.toolbarHeightConstraints.count) {
      heightConstraint.additionalHeight = self.additionalStackHeight;
      heightConstraint.collapsesAdditionalHeight = self.collapsesSafeArea;
    }
    [_toolbarHeightConstraints addObject:heightConstraint];
  }
}

// Updates the height of the first toolbar to account for the safe area.
- (void)updateForSafeArea {
  if (@available(iOS 11, *)) {
    if (self.orientation == ToolbarContainerOrientation::kTopToBottom) {
      self.additionalStackHeight = self.view.safeAreaInsets.top;
    } else {
      self.additionalStackHeight = self.view.safeAreaInsets.bottom;
    }
  } else {
    if (self.orientation == ToolbarContainerOrientation::kTopToBottom) {
      self.additionalStackHeight = self.topLayoutGuide.length;
    } else {
      self.additionalStackHeight = self.bottomLayoutGuide.length;
    }
  }
}

// Updates the height constraint's constant to the cumulative expanded height of
// all the toolbars.
- (void)updateHeightConstraint {
  if (!self.viewLoaded)
    return;
  // Calculate the cumulative expanded toolbar height.
  CGFloat cumulativeExpandedHeight = 0.0;
  for (CollapsingToolbarHeightConstraint* constraint in self
           .toolbarHeightConstraints) {
    cumulativeExpandedHeight +=
        constraint.expandedHeight + constraint.additionalHeight;
  }
  self.heightConstraint.constant = cumulativeExpandedHeight;
}

@end
