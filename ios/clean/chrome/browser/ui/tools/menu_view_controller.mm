// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"

#include "base/i18n/rtl.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/macros.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button.h"
#import "ios/clean/chrome/browser/ui/tools/menu_overflow_controls_stackview.h"
#import "ios/clean/chrome/browser/ui/tools/tools_actions.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMenuWidth = 250;
const CGFloat kMenuItemHeight = 48;
}

@interface MenuViewController ()<ToolsActions>
@property(nonatomic, strong) UIStackView* menuStackView;
@property(nonatomic, strong) NSArray<ToolsMenuItem*>* menuItems;
@property(nonatomic, strong)
    MenuOverflowControlsStackView* toolbarOverflowStackView;
@property(nonatomic, assign) BOOL displayOverflowControls;
@end

@implementation MenuViewController
@synthesize dispatcher = _dispatcher;
@synthesize menuItems = _menuItems;
@synthesize menuStackView = _menuStackView;
@synthesize toolbarOverflowStackView = _toolbarOverflowStackView;
@synthesize displayOverflowControls = _displayOverflowControls;

- (void)loadView {
  CGRect frame;
  frame.size = CGSizeMake(kMenuWidth, kMenuItemHeight * _menuItems.count);
  frame.origin = CGPointZero;
  self.view = [[UIView alloc] initWithFrame:frame];
  self.view.backgroundColor = [UIColor whiteColor];
  self.view.autoresizingMask = UIViewAutoresizingNone;
  self.view.layer.borderColor = [UIColor clearColor].CGColor;
}

- (void)viewDidLoad {
  NSMutableArray<UIButton*>* buttons =
      [[NSMutableArray alloc] initWithCapacity:_menuItems.count];

  // Load menu items.
  for (ToolsMenuItem* item in _menuItems) {
    UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    menuButton.tintColor = [UIColor blackColor];
    [menuButton setTitle:item.title forState:UIControlStateNormal];
    [menuButton setContentEdgeInsets:UIEdgeInsetsMakeDirected(0, 10.0f, 0, 0)];
    [menuButton.titleLabel setFont:[MDCTypography subheadFont]];
    [menuButton.titleLabel setTextAlignment:NSTextAlignmentNatural];
    [menuButton addTarget:self.dispatcher
                   action:@selector(closeToolsMenu)
         forControlEvents:UIControlEventTouchUpInside];
    if (item.action) {
      id target =
          (item.action == @selector(showFindInPage)) ? self.dispatcher : nil;
      [menuButton addTarget:target
                     action:item.action
           forControlEvents:UIControlEventTouchUpInside];
    }
    [buttons addObject:menuButton];
  }

  // Placeholder stack view to hold menu contents.
  self.menuStackView = [[UIStackView alloc] initWithArrangedSubviews:buttons];
  self.menuStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.menuStackView.axis = UILayoutConstraintAxisVertical;
  self.menuStackView.distribution = UIStackViewDistributionFillEqually;
  self.menuStackView.alignment = UIStackViewAlignmentLeading;

  // Stack view to hold overflow ToolbarButtons.
  if (self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      self.displayOverflowControls) {
    [self setUpOverFlowControlsStackView];
  }

  // Setup constraints.
  [self.view addSubview:self.menuStackView];
  [NSLayoutConstraint activateConstraints:@[
    [self.menuStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.menuStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.menuStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.menuStackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
  ]];
}

- (void)setUpOverFlowControlsStackView {
  self.toolbarOverflowStackView = [[MenuOverflowControlsStackView alloc] init];
  // PLACEHOLDER: ToolsMenuButton might end up being part of the MenuVC's view
  // instead of the StackView. We are waiting confirmation on this.
  for (UIView* view in self.toolbarOverflowStackView.arrangedSubviews) {
    if ([view isKindOfClass:[ToolbarButton class]]) {
      ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
      [button addTarget:self.dispatcher
                    action:@selector(closeToolsMenu)
          forControlEvents:UIControlEventTouchUpInside];
    }
  }
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
  [NSLayoutConstraint activateConstraints:@[
    [self.toolbarOverflowStackView.leadingAnchor
        constraintEqualToAnchor:self.menuStackView.leadingAnchor],
    [self.toolbarOverflowStackView.trailingAnchor
        constraintEqualToAnchor:self.menuStackView.trailingAnchor],
  ]];
}

#pragma mark - Tools Consumer

- (void)setToolsMenuItems:(NSArray*)menuItems {
  _menuItems = menuItems;
}

- (void)displayOverflowControls:(BOOL)displayOverflowControls {
  self.displayOverflowControls = displayOverflowControls;
}

@end
