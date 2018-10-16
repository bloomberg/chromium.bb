// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view_controller.h"

#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The container view width.
const CGFloat kContainerViewWidth = 300.0;
// The expanded height of the collapsing toolbar.
const CGFloat kExpandedToolbarHeight = 100.0;
// The collapsed height of the collapsing toolbar.
const CGFloat kCollapsedToolbarHeight = 50.0;
// The height of the non-collapsing toolbar.
const CGFloat kNonCollapsingToolbarHeight = 75.0;
}  // namespace

// Test toolbar view.
@interface TestToolbarView : UIView<ToolbarCollapsing>
// Redefine ToolbarCollapsing properies as readwrite.
@property(nonatomic, assign, readwrite) CGFloat expandedToolbarHeight;
@property(nonatomic, assign, readwrite) CGFloat collapsedToolbarHeight;
@end

@implementation TestToolbarView
@synthesize expandedToolbarHeight = _expandedToolbarHeight;
@synthesize collapsedToolbarHeight = _collapsedToolbarHeight;
@end

// Test toolbar view controller.
@interface TestToolbarViewController : UIViewController
@property(nonatomic, strong, readonly) TestToolbarView* toolbarView;
@property(nonatomic, assign) CGFloat expandedToolbarHeight;
@property(nonatomic, assign) CGFloat collapsedToolbarHeight;
@end

@implementation TestToolbarViewController
@synthesize expandedToolbarHeight = _expandedToolbarHeight;
@synthesize collapsedToolbarHeight = _collapsedToolbarHeight;
- (void)loadView {
  TestToolbarView* view = [[TestToolbarView alloc] initWithFrame:CGRectZero];
  view.expandedToolbarHeight = self.expandedToolbarHeight;
  view.collapsedToolbarHeight = self.collapsedToolbarHeight;
  self.view = view;
}
- (TestToolbarView*)toolbarView {
  return static_cast<TestToolbarView*>(self.view);
}
@end

// Test fixture for ToolbarContainerViewController.
class ToolbarContainerViewControllerTest : public PlatformTest {
 public:
  ToolbarContainerViewControllerTest()
      : PlatformTest(),
        window_([[UIWindow alloc] init]),
        view_controller_([[ToolbarContainerViewController alloc] init]),
        collapsing_toolbar_([[TestToolbarViewController alloc] init]),
        non_collapsing_toolbar_([[TestToolbarViewController alloc] init]) {
    collapsing_toolbar_.expandedToolbarHeight = kExpandedToolbarHeight;
    collapsing_toolbar_.collapsedToolbarHeight = kCollapsedToolbarHeight;
    non_collapsing_toolbar_.expandedToolbarHeight = kNonCollapsingToolbarHeight;
    non_collapsing_toolbar_.collapsedToolbarHeight =
        kNonCollapsingToolbarHeight;
    [container_view().widthAnchor constraintEqualToConstant:kContainerViewWidth]
        .active = YES;
    window_.frame = CGRectMake(0.0, 0.0, kContainerViewWidth, 1000);
    [window_ addSubview:container_view()];
    AddSameConstraintsToSides(window_, container_view(),
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    SetOrientation(ToolbarContainerOrientation::kTopToBottom);
    view_controller_.toolbars =
        @[ non_collapsing_toolbar_, collapsing_toolbar_ ];
    ForceLayout();
  }

  ~ToolbarContainerViewControllerTest() override {
    view_controller_.toolbars = nil;
  }

  // Returns the additional stack height created by the safe area insets.
  CGFloat GetAdditionalStackHeight() {
    CGFloat additional_height = 0.0;
    bool top_to_bottom = view_controller_.orientation ==
                         ToolbarContainerOrientation::kTopToBottom;
    if (@available(iOS 11, *)) {
      additional_height = top_to_bottom
                              ? container_view().safeAreaInsets.top
                              : container_view().safeAreaInsets.bottom;
    } else if (top_to_bottom) {
      additional_height = view_controller_.topLayoutGuide.length;
    }
    return additional_height;
  }

  // Returns the expected height of the container.
  CGFloat GetExpectedContainerHeight() {
    return kNonCollapsingToolbarHeight + kExpandedToolbarHeight +
           GetAdditionalStackHeight();
  }

  // Expand or collapse the toolbars.
  void SetExpanded(bool expanded) {
    [view_controller_ updateForFullscreenProgress:expanded ? 1.0 : 0.0];
    ForceLayout();
  }

  // Sets the container orientation.
  void SetOrientation(ToolbarContainerOrientation orientation) {
    container_positioning_constraint_.active = NO;
    view_controller_.orientation = orientation;
    if (orientation == ToolbarContainerOrientation::kTopToBottom) {
      container_positioning_constraint_ = [container_view().topAnchor
          constraintEqualToAnchor:window_.topAnchor];
    } else {
      container_positioning_constraint_ = [container_view().bottomAnchor
          constraintEqualToAnchor:window_.bottomAnchor];
    }
    container_positioning_constraint_.active = YES;
    ForceLayout();
  }

  // Sets whether the safe area should be collapsed.
  void SetCollapsesSafeArea(bool collapses_safe_area) {
    view_controller_.collapsesSafeArea = collapses_safe_area;
  }

  // Sets the safe area insets or top layout guide for the container and forces
  // a layout.
  void SetSafeAreaInsets(UIEdgeInsets insets) {
    if (@available(iOS 11, *)) {
      view_controller_.additionalSafeAreaInsets = insets;
    } else {
      // Deactivate all pre-existing constraints for the layout guides' heights.
      // They are added by UIKit at the maximum priority, so must be removed to
      // update the lengths of the layout guides.
      for (NSLayoutConstraint* constraint in container_view().constraints) {
        if (constraint.firstAttribute == NSLayoutAttributeHeight &&
            (constraint.firstItem == view_controller_.topLayoutGuide ||
             constraint.firstItem == view_controller_.bottomLayoutGuide)) {
          constraint.active = NO;
        }
      }
      [view_controller_.topLayoutGuide.heightAnchor
          constraintEqualToConstant:insets.top]
          .active = YES;
      [view_controller_.bottomLayoutGuide.heightAnchor
          constraintEqualToConstant:insets.bottom]
          .active = YES;
    }
    ForceLayout();
  }

  // Forces a layout of the hierarchy.
  void ForceLayout() {
    [window_ setNeedsLayout];
    [window_ layoutIfNeeded];
    [container_view() setNeedsLayout];
    [container_view() layoutIfNeeded];
  }

  // The views.
  UIView* container_view() { return view_controller_.view; }
  TestToolbarView* collapsing_toolbar_view() {
    return collapsing_toolbar_.toolbarView;
  }
  TestToolbarView* non_collapsing_toolbar_view() {
    return non_collapsing_toolbar_.toolbarView;
  }
  TestToolbarView* first_toolbar_view() {
    return static_cast<TestToolbarViewController*>(view_controller_.toolbars[0])
        .toolbarView;
  }

 private:
  __strong UIWindow* window_ = nil;
  __strong ToolbarContainerViewController* view_controller_ = nil;
  __strong NSLayoutConstraint* container_positioning_constraint_ = nil;
  __strong TestToolbarViewController* collapsing_toolbar_ = nil;
  __strong TestToolbarViewController* non_collapsing_toolbar_ = nil;
};

// Tests the layout of the toolbar views in when oriented from top to bottom
// and the toolbars are fully expanded.
TEST_F(ToolbarContainerViewControllerTest, TopToBottomExpanded) {
  SetOrientation(ToolbarContainerOrientation::kTopToBottom);
  SetExpanded(true);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  CGRect non_collapsing_toolbar_frame =
      CGRectMake(0.0, 0.0, kContainerViewWidth,
                 kNonCollapsingToolbarHeight + GetAdditionalStackHeight());
  EXPECT_TRUE(CGRectEqualToRect(non_collapsing_toolbar_frame,
                                non_collapsing_toolbar_view().frame));
  CGRect collapsing_toolbar_frame =
      CGRectMake(0.0, CGRectGetMaxY(non_collapsing_toolbar_frame),
                 kContainerViewWidth, kExpandedToolbarHeight);
  EXPECT_TRUE(CGRectEqualToRect(collapsing_toolbar_frame,
                                collapsing_toolbar_view().frame));
}

// Tests the layout of the toolbar views in when oriented from top to bottom
// and the toolbars are fully collapsed.
TEST_F(ToolbarContainerViewControllerTest, TopToBottomCollapsed) {
  SetOrientation(ToolbarContainerOrientation::kTopToBottom);
  SetExpanded(false);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  CGRect non_collapsing_toolbar_frame =
      CGRectMake(0.0, 0.0, kContainerViewWidth,
                 kNonCollapsingToolbarHeight + GetAdditionalStackHeight());
  EXPECT_TRUE(CGRectEqualToRect(non_collapsing_toolbar_frame,
                                non_collapsing_toolbar_view().frame));
  CGRect collapsing_toolbar_frame =
      CGRectMake(0.0, CGRectGetMaxY(non_collapsing_toolbar_frame),
                 kContainerViewWidth, kCollapsedToolbarHeight);
  EXPECT_TRUE(CGRectEqualToRect(collapsing_toolbar_frame,
                                collapsing_toolbar_view().frame));
}

// Tests the layout of the toolbar views in when oriented from bottom to top
// and the toolbars are fully expanded.
TEST_F(ToolbarContainerViewControllerTest, BottomToTopExpanded) {
  SetOrientation(ToolbarContainerOrientation::kBottomToTop);
  SetExpanded(true);
  CGFloat container_height = CGRectGetHeight(container_view().bounds);
  EXPECT_EQ(GetExpectedContainerHeight(), container_height);
  CGRect non_collapsing_toolbar_frame = CGRectMake(
      0.0, container_height - kNonCollapsingToolbarHeight, kContainerViewWidth,
      kNonCollapsingToolbarHeight + GetAdditionalStackHeight());
  EXPECT_TRUE(CGRectEqualToRect(non_collapsing_toolbar_frame,
                                non_collapsing_toolbar_view().frame));
  CGRect collapsing_toolbar_frame = CGRectMake(
      0.0, CGRectGetMinY(non_collapsing_toolbar_frame) - kExpandedToolbarHeight,
      kContainerViewWidth, kExpandedToolbarHeight);
  EXPECT_TRUE(CGRectEqualToRect(collapsing_toolbar_frame,
                                collapsing_toolbar_view().frame));
}

// Tests the layout of the toolbar views in when oriented from bottom to top
// and the toolbars are fully collapsed.
TEST_F(ToolbarContainerViewControllerTest, BottomToTopCollapsed) {
  SetOrientation(ToolbarContainerOrientation::kBottomToTop);
  SetExpanded(false);
  CGFloat container_height = CGRectGetHeight(container_view().bounds);
  EXPECT_EQ(GetExpectedContainerHeight(), container_height);
  CGRect non_collapsing_toolbar_frame = CGRectMake(
      0.0, container_height - kNonCollapsingToolbarHeight, kContainerViewWidth,
      kNonCollapsingToolbarHeight + GetAdditionalStackHeight());
  EXPECT_TRUE(CGRectEqualToRect(non_collapsing_toolbar_frame,
                                non_collapsing_toolbar_view().frame));
  CGRect collapsing_toolbar_frame = CGRectMake(
      0.0,
      CGRectGetMinY(non_collapsing_toolbar_frame) - kCollapsedToolbarHeight,
      kContainerViewWidth, kCollapsedToolbarHeight);
  EXPECT_TRUE(CGRectEqualToRect(collapsing_toolbar_frame,
                                collapsing_toolbar_view().frame));
}

// Tests that the container and the top toolbar's height accounts for the non-
// collapsing safe area.
TEST_F(ToolbarContainerViewControllerTest, NonCollapsingTopSafeArea) {
  const UIEdgeInsets kSafeInsets = UIEdgeInsetsMake(100.0, 0.0, 0.0, 0.0);
  SetCollapsesSafeArea(false);
  SetSafeAreaInsets(kSafeInsets);
  SetOrientation(ToolbarContainerOrientation::kTopToBottom);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  SetExpanded(true);
  TestToolbarView* toolbar_view = first_toolbar_view();
  EXPECT_EQ(toolbar_view.expandedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
  SetExpanded(false);
  EXPECT_EQ(toolbar_view.collapsedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
}

// Tests that the container and the bottom toolbar's height accounts for the
// non-collapsing safe area.
TEST_F(ToolbarContainerViewControllerTest, NonCollapsingBottomSafeArea) {
  const UIEdgeInsets kSafeInsets = UIEdgeInsetsMake(100.0, 0.0, 0.0, 0.0);
  SetCollapsesSafeArea(false);
  SetSafeAreaInsets(kSafeInsets);
  SetOrientation(ToolbarContainerOrientation::kBottomToTop);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  SetExpanded(true);
  TestToolbarView* toolbar_view = first_toolbar_view();
  EXPECT_EQ(toolbar_view.expandedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
  SetExpanded(false);
  EXPECT_EQ(toolbar_view.collapsedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
}

// Tests that the container and the top toolbar's height accounts for the
// collapsing safe area.
TEST_F(ToolbarContainerViewControllerTest, CollapsingTopSafeArea) {
  const UIEdgeInsets kSafeInsets = UIEdgeInsetsMake(100.0, 0.0, 0.0, 0.0);
  SetCollapsesSafeArea(true);
  SetSafeAreaInsets(kSafeInsets);
  SetOrientation(ToolbarContainerOrientation::kTopToBottom);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  SetExpanded(true);
  TestToolbarView* toolbar_view = first_toolbar_view();
  EXPECT_EQ(toolbar_view.expandedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
  SetExpanded(false);
  EXPECT_EQ(toolbar_view.collapsedToolbarHeight,
            CGRectGetHeight(toolbar_view.frame));
}

// Tests that the container and the bottom toolbar's height accounts for the
// collapsing safe area.
TEST_F(ToolbarContainerViewControllerTest, CollapsingBottomSafeArea) {
  const UIEdgeInsets kSafeInsets = UIEdgeInsetsMake(100.0, 0.0, 0.0, 0.0);
  SetCollapsesSafeArea(true);
  SetSafeAreaInsets(kSafeInsets);
  SetOrientation(ToolbarContainerOrientation::kBottomToTop);
  EXPECT_EQ(GetExpectedContainerHeight(),
            CGRectGetHeight(container_view().bounds));
  SetExpanded(true);
  TestToolbarView* toolbar_view = first_toolbar_view();
  EXPECT_EQ(toolbar_view.expandedToolbarHeight + GetAdditionalStackHeight(),
            CGRectGetHeight(toolbar_view.frame));
  SetExpanded(false);
  EXPECT_EQ(toolbar_view.collapsedToolbarHeight,
            CGRectGetHeight(toolbar_view.frame));
}
