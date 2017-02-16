// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"

#include "base/i18n/rtl.h"
#import "base/logging.h"
#import "base/macros.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tools_menu_actions.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button.h"
#import "ios/clean/chrome/browser/ui/tools/menu_overflow_controls_stackview.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMenuWidth = 250;
const CGFloat kMenuItemHeight = 48;
}

// Placeholder model for menu item configuration.
@interface MenuItem : NSObject
@property(nonatomic, copy) NSString* title;
@property(nonatomic) SEL action;
@end

@implementation MenuItem
@synthesize title = _title;
@synthesize action = _action;
@end

@interface MenuViewController ()
@property(nonatomic, readonly) NSArray<MenuItem*>* menuItems;
@property(nonatomic, strong)
    MenuOverflowControlsStackView* toolbarOverflowStackView;
@end

@implementation MenuViewController
@synthesize menuItems = _menuItems;
@synthesize toolbarOverflowStackView = _toolbarOverflowStackView;

- (instancetype)init {
  if ((self = [super init])) {
    _menuItems = @[
      [[MenuItem alloc] init], [[MenuItem alloc] init], [[MenuItem alloc] init],
      [[MenuItem alloc] init], [[MenuItem alloc] init], [[MenuItem alloc] init],
      [[MenuItem alloc] init], [[MenuItem alloc] init], [[MenuItem alloc] init],
      [[MenuItem alloc] init], [[MenuItem alloc] init]
    ];

    _menuItems[0].title = @"New Tab";

    _menuItems[1].title = @"New Incognito Tab";

    _menuItems[2].title = @"Bookmarks";

    _menuItems[3].title = @"Reading List";

    _menuItems[4].title = @"Recent Tabs";

    _menuItems[5].title = @"History";

    _menuItems[6].title = @"Report an Issue";

    _menuItems[7].title = @"Find in Page…";

    _menuItems[8].title = @"Request Desktop Site";

    _menuItems[9].title = @"Settings";
    _menuItems[9].action = @selector(showSettings:);

    _menuItems[10].title = @"Help";
  }
  return self;
}

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

  for (MenuItem* item in _menuItems) {
    UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    menuButton.tintColor = [UIColor blackColor];
    [menuButton setTitle:item.title forState:UIControlStateNormal];
    [menuButton setContentEdgeInsets:UIEdgeInsetsMakeDirected(0, 10.0f, 0, 0)];
    [menuButton.titleLabel
        setFont:[[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16]];
    [menuButton.titleLabel setTextAlignment:NSTextAlignmentNatural];
    [menuButton addTarget:nil
                   action:@selector(closeToolsMenu:)
         forControlEvents:UIControlEventTouchUpInside];
    if (item.action) {
      [menuButton addTarget:nil
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

@end
