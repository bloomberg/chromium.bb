// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"

#include "base/i18n/rtl.h"
#import "base/logging.h"
#import "base/macros.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
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
@property(nonatomic, strong) NSArray<ToolsMenuItem*>* menuItems;
@property(nonatomic, strong)
    MenuOverflowControlsStackView* toolbarOverflowStackView;
@end

@implementation MenuViewController
@synthesize dispatcher = _dispatcher;
@synthesize menuItems = _menuItems;
@synthesize toolbarOverflowStackView = _toolbarOverflowStackView;

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

  for (ToolsMenuItem* item in _menuItems) {
    UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    menuButton.tintColor = [UIColor blackColor];
    [menuButton setTitle:item.title forState:UIControlStateNormal];
    [menuButton setContentEdgeInsets:UIEdgeInsetsMakeDirected(0, 10.0f, 0, 0)];
    [menuButton.titleLabel setFont:[MDCTypography subheadFont]];
    [menuButton.titleLabel setTextAlignment:NSTextAlignmentNatural];
    [menuButton addTarget:self
                   action:@selector(closeToolsMenu:)
         forControlEvents:UIControlEventTouchUpInside];
    if (item.action) {
      id target = (item.action == @selector(showFindInPage)) ? self : nil;
      [menuButton addTarget:target
                     action:item.action
           forControlEvents:UIControlEventTouchUpInside];
    }
    [buttons addObject:menuButton];
  }

  // Placeholder stack view to hold menu contents.
  UIStackView* menu = [[UIStackView alloc] initWithArrangedSubviews:buttons];
  menu.translatesAutoresizingMaskIntoConstraints = NO;
  menu.axis = UILayoutConstraintAxisVertical;
  menu.distribution = UIStackViewDistributionFillEqually;
  menu.alignment = UIStackViewAlignmentLeading;

  // Stack view to hold overflow ToolbarButtons.
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    self.toolbarOverflowStackView =
        [[MenuOverflowControlsStackView alloc] init];
    // PLACEHOLDER: ToolsMenuButton might end up being part of the MenuVC's view
    // instead of the StackView. We are waiting confirmation on this.
    [self.toolbarOverflowStackView.toolsMenuButton
               addTarget:nil
                  action:@selector(closeToolsMenu:)
        forControlEvents:UIControlEventTouchUpInside];
    [menu insertArrangedSubview:self.toolbarOverflowStackView atIndex:0];
    [NSLayoutConstraint activateConstraints:@[
      [self.toolbarOverflowStackView.leadingAnchor
          constraintEqualToAnchor:menu.leadingAnchor],
      [self.toolbarOverflowStackView.trailingAnchor
          constraintEqualToAnchor:menu.trailingAnchor],
    ]];
  }

  [self.view addSubview:menu];
  [NSLayoutConstraint activateConstraints:@[
    [menu.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [menu.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [menu.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [menu.topAnchor constraintEqualToAnchor:self.view.topAnchor],
  ]];
}

#pragma mark - ToolsMenuCommands

- (void)closeToolsMenu:(id)sender {
  [self.dispatcher closeToolsMenu];
}

- (void)showFindInPage {
  [self.dispatcher showFindInPage];
}

#pragma mark - Tools Consumer

- (void)setToolsMenuItems:(NSArray*)menuItems {
  _menuItems = menuItems;
}

@end
