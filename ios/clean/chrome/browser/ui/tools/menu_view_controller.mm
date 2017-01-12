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

@implementation MenuViewController

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor whiteColor];
  struct MenuItem {
    NSString* title;
    SEL action;
  };
  MenuItem menuItems[] = {
      {@"New Tab", nullptr},
      {@"Find in Page…", nullptr},
      {@"Request Desktop Site", nullptr},
      {@"Settings", @selector(showSettings:)},
  };
  NSMutableArray<UIButton*>* buttons =
      [[NSMutableArray alloc] initWithCapacity:arraysize(menuItems)];

  for (size_t i = 0; i < arraysize(menuItems); ++i) {
    const MenuItem& item = menuItems[i];
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
