// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tools/menu_view_controller.h"

#import "base/logging.h"
#import "base/macros.h"
#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tools_menu_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMenuWidth = 250;
const CGFloat kMenuItemHeight = 44;
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
@end

@implementation MenuViewController
@synthesize menuItems = _menuItems;

- (instancetype)init {
  if ((self = [super init])) {
    _menuItems = @[
      [[MenuItem alloc] init], [[MenuItem alloc] init], [[MenuItem alloc] init],
      [[MenuItem alloc] init]
    ];

    _menuItems[0].title = @"New Tab";

    _menuItems[1].title = @"Find in Page…";

    _menuItems[2].title = @"Request Desktop Site";

    _menuItems[3].title = @"Settings";
    _menuItems[3].action = @selector(showSettings:);
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
}

- (void)viewDidLoad {
  NSMutableArray<UIButton*>* buttons =
      [[NSMutableArray alloc] initWithCapacity:_menuItems.count];

  for (MenuItem* item in _menuItems) {
    UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [menuButton setTitle:item.title forState:UIControlStateNormal];
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

  [self.view addSubview:menu];
  [NSLayoutConstraint activateConstraints:@[
    [menu.leadingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.leadingAnchor],
    [menu.trailingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.trailingAnchor],
    [menu.bottomAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.bottomAnchor],
    [menu.topAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.topAnchor],
  ]];
}

@end
