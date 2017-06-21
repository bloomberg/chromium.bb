// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"

#include "base/i18n/rtl.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/macros.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button+factory.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_constants.h"
#import "ios/clean/chrome/browser/ui/tools/menu_overflow_controls_stackview.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMenuWidth = 250.0;
const CGFloat kMenuItemHeight = 48.0;
const CGFloat kMenuItemLeadingEdgeInset = 12.0;
const CGFloat kOverflowControlsMargin = 58.0;
const CGFloat kCloseButtonHeight = 44.0;
}

@interface MenuViewController ()
@property(nonatomic, strong) UIScrollView* menuScrollView;
@property(nonatomic, strong) UIStackView* menuStackView;
@property(nonatomic, strong) NSArray<ToolsMenuItem*>* menuItems;
@property(nonatomic, strong)
    MenuOverflowControlsStackView* toolbarOverflowStackView;
@property(nonatomic, assign) BOOL displayOverflowControls;
@property(nonatomic, strong) ToolbarButton* closeMenuButton;
@property(nonatomic, assign) BOOL currentPageLoading;

// Sets up the main StackView and creates a button for each Menu item.
- (void)setupMenuStackView;
// Sets up the Overflow navigation controls stack view.
- (void)setUpOverFlowControlsStackView;
// Sets up and activates all the View constraints.
- (void)setupConstraints;
@end

@implementation MenuViewController
@synthesize dispatcher = _dispatcher;
@synthesize menuItems = _menuItems;
@synthesize menuStackView = _menuStackView;
@synthesize toolbarOverflowStackView = _toolbarOverflowStackView;
@synthesize displayOverflowControls = _displayOverflowControls;
@synthesize menuScrollView = _menuScrollView;
@synthesize closeMenuButton = _closeMenuButton;
@synthesize currentPageLoading = _currentPageLoading;

#pragma mark - View Lifecycle

- (void)loadView {
  CGRect frame;
  // Set the MenuVC view height depending on the current screen height.
  CGFloat screenHeight = [UIScreen mainScreen].bounds.size.height;
  CGFloat itemsHeight = kMenuItemHeight * _menuItems.count;
  CGFloat menuHeight =
      itemsHeight > screenHeight ? screenHeight - kMenuItemHeight : itemsHeight;
  frame.size = CGSizeMake(kMenuWidth, menuHeight);
  frame.origin = CGPointZero;
  self.view = [[UIView alloc] initWithFrame:frame];
  self.view.backgroundColor = [UIColor whiteColor];
  self.view.autoresizingMask = UIViewAutoresizingNone;
  self.view.layer.borderColor = [UIColor clearColor].CGColor;
}

- (void)viewDidLoad {
  self.menuScrollView = [[UIScrollView alloc] init];
  self.menuScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.menuScrollView];

  [self setupCloseMenuButton];
  [self setupMenuStackView];
  [self setupConstraints];
}

#pragma mark - UI Setup

- (void)setupCloseMenuButton {
  // Add close tools menu button. This button is fixed on the top right corner
  // and will always be visible.
  self.closeMenuButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:
          NativeImage(IDR_IOS_TOOLBAR_LIGHT_TOOLS_PRESSED)
                  imageForHighlightedState:NativeImage(
                                               IDR_IOS_TOOLBAR_LIGHT_TOOLS)
                     imageForDisabledState:nil];
  [self.closeMenuButton
      setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
  [self.closeMenuButton addTarget:self.dispatcher
                           action:@selector(closeToolsMenu)
                 forControlEvents:UIControlEventTouchUpInside];

  NSLayoutConstraint* widthConstraint = [self.closeMenuButton.widthAnchor
      constraintEqualToConstant:kToolbarButtonWidth];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* heightConstraint = [self.closeMenuButton.heightAnchor
      constraintEqualToConstant:kCloseButtonHeight];
  heightConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint
      activateConstraints:@[ widthConstraint, heightConstraint ]];
  [self.view addSubview:self.closeMenuButton];
}

- (void)setupMenuStackView {
  NSMutableArray<UIButton*>* buttons =
      [[NSMutableArray alloc] initWithCapacity:_menuItems.count];
  // Load menu items.
  for (ToolsMenuItem* item in _menuItems) {
    UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    menuButton.tintColor = [UIColor blackColor];
    // Button constraints will be changed in order to match the menu width, for
    // this reason the content will be centered if no content alignment is set.
    menuButton.contentHorizontalAlignment =
        UseRTLLayout() ? UIControlContentHorizontalAlignmentRight
                       : UIControlContentHorizontalAlignmentLeft;
    [menuButton setTitle:item.title forState:UIControlStateNormal];
    [menuButton setContentEdgeInsets:UIEdgeInsetsMakeDirected(
                                         0, kMenuItemLeadingEdgeInset, 0, 0)];
    [menuButton.titleLabel setFont:[MDCTypography subheadFont]];
    [menuButton.titleLabel setTextAlignment:NSTextAlignmentNatural];
    [menuButton addTarget:self.dispatcher
                   action:@selector(closeToolsMenu)
         forControlEvents:UIControlEventTouchUpInside];
    if (item.action) {
      [menuButton addTarget:self.dispatcher
                     action:item.action
           forControlEvents:UIControlEventTouchUpInside];
    }
    [buttons addObject:menuButton];
  }

  self.menuStackView = [[UIStackView alloc] initWithArrangedSubviews:buttons];
  self.menuStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.menuStackView.axis = UILayoutConstraintAxisVertical;
  self.menuStackView.distribution = UIStackViewDistributionFillEqually;
  self.menuStackView.alignment = UIStackViewAlignmentLeading;

  // Set button constraints so they have the same width as the StackView that
  // contains them.
  NSMutableArray* buttonConstraints = [[NSMutableArray alloc] init];
  for (UIView* view in self.menuStackView.arrangedSubviews) {
    [buttonConstraints
        addObject:[view.leadingAnchor
                      constraintEqualToAnchor:self.menuStackView
                                                  .leadingAnchor]];
    [buttonConstraints
        addObject:[view.trailingAnchor
                      constraintEqualToAnchor:self.menuStackView
                                                  .trailingAnchor]];
  }
  [NSLayoutConstraint activateConstraints:buttonConstraints];

  // Stack view to hold overflow ToolbarButtons.
  if (self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      self.displayOverflowControls) {
    [self setUpOverFlowControlsStackView];
  }
}

- (void)setUpOverFlowControlsStackView {
  self.toolbarOverflowStackView = [[MenuOverflowControlsStackView alloc] init];
  for (UIView* view in self.toolbarOverflowStackView.arrangedSubviews) {
    if ([view isKindOfClass:[ToolbarButton class]]) {
      ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
      [button addTarget:self.dispatcher
                    action:@selector(closeToolsMenu)
          forControlEvents:UIControlEventTouchUpInside];
    }
  }
  self.toolbarOverflowStackView.reloadButton.hidden = self.currentPageLoading;
  self.toolbarOverflowStackView.stopButton.hidden = !self.currentPageLoading;
  [self.toolbarOverflowStackView.reloadButton
             addTarget:self.dispatcher
                action:@selector(reloadPage)
      forControlEvents:UIControlEventTouchUpInside];
  [self.toolbarOverflowStackView.stopButton
             addTarget:self.dispatcher
                action:@selector(stopLoadingPage)
      forControlEvents:UIControlEventTouchUpInside];

  [self.menuStackView insertArrangedSubview:self.toolbarOverflowStackView
                                    atIndex:0];
  NSLayoutConstraint* leadingConstraint =
      [self.toolbarOverflowStackView.leadingAnchor
          constraintEqualToAnchor:self.menuStackView.leadingAnchor];
  leadingConstraint.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* trailingConstraint =
      [self.toolbarOverflowStackView.trailingAnchor
          constraintEqualToAnchor:self.menuStackView.trailingAnchor
                         constant:-kOverflowControlsMargin];
  trailingConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint
      activateConstraints:@[ leadingConstraint, trailingConstraint ]];
}

- (void)setupConstraints {
  [self.menuScrollView addSubview:self.menuStackView];
  [NSLayoutConstraint activateConstraints:@[
    // ScrollView Constraints.
    [self.menuScrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.menuScrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.menuScrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.menuScrollView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // StackView Constraints.
    [self.menuStackView.leadingAnchor
        constraintEqualToAnchor:self.menuScrollView.leadingAnchor],
    [self.menuStackView.trailingAnchor
        constraintEqualToAnchor:self.menuScrollView.trailingAnchor],
    [self.menuStackView.bottomAnchor
        constraintEqualToAnchor:self.menuScrollView.bottomAnchor],
    [self.menuStackView.topAnchor
        constraintEqualToAnchor:self.menuScrollView.topAnchor],
    [self.menuStackView.widthAnchor
        constraintEqualToAnchor:self.menuScrollView.widthAnchor],
    [self.menuStackView.heightAnchor
        constraintEqualToConstant:kMenuItemHeight * self.menuItems.count],
    // CloseMenu Button Constraint.
    [self.closeMenuButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.closeMenuButton.topAnchor
        constraintEqualToAnchor:self.menuScrollView.topAnchor],
  ]];
}

#pragma mark - Tools Consumer

- (void)setToolsMenuItems:(NSArray*)menuItems {
  _menuItems = menuItems;
}

- (void)displayOverflowControls:(BOOL)displayOverflowControls {
  self.displayOverflowControls = displayOverflowControls;
}

- (void)setIsLoading:(BOOL)isLoading {
  self.currentPageLoading = isLoading;
}

- (void)setCurrentPageLoading:(BOOL)currentPageLoading {
  _currentPageLoading = currentPageLoading;
  // If the OverflowButtons have been initialized update their visibility.
  if (self.toolbarOverflowStackView) {
    self.toolbarOverflowStackView.reloadButton.hidden = currentPageLoading;
    self.toolbarOverflowStackView.stopButton.hidden = !currentPageLoading;
  }
}

@end
