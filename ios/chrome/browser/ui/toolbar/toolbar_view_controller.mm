// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#import "ios/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/chrome/browser/ui/actions/tools_menu_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarViewController ()<ToolsMenuActions>
@property(nonatomic, weak) UITextField* omnibox;
@property(nonatomic, weak) UIButton* toolsMenu;
@end

@implementation ToolbarViewController
@synthesize actionDelegate = _actionDelegate;
@synthesize omnibox = _omnibox;
@synthesize toolsMenu = _toolsMenu;

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor lightGrayColor];

  // Tab switcher button.
  UIButton* tabSwitcher = [UIButton buttonWithType:UIButtonTypeSystem];
  tabSwitcher.translatesAutoresizingMaskIntoConstraints = NO;
  [tabSwitcher setTitle:@"⊞" forState:UIControlStateNormal];
  tabSwitcher.titleLabel.font = [UIFont systemFontOfSize:24.0];
  [tabSwitcher addTarget:nil
                  action:@selector(showTabGrid:)
        forControlEvents:UIControlEventTouchUpInside];

  // Placeholder omnibox.
  UITextField* omnibox = [[UITextField alloc] initWithFrame:CGRectZero];
  omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  omnibox.backgroundColor = [UIColor whiteColor];
  omnibox.enabled = NO;
  self.omnibox = omnibox;

  // Tools menu button.
  UIButton* toolsMenu = [UIButton buttonWithType:UIButtonTypeSystem];
  toolsMenu.translatesAutoresizingMaskIntoConstraints = NO;
  [toolsMenu setTitle:@"⋮" forState:UIControlStateNormal];
  toolsMenu.titleLabel.font = [UIFont systemFontOfSize:24.0];
  [toolsMenu addTarget:nil
                action:@selector(showToolsMenu:)
      forControlEvents:UIControlEventTouchUpInside];
  self.toolsMenu = toolsMenu;

  // Stack view to contain toolbar items.
  UIStackView* toolbarItems = [[UIStackView alloc]
      initWithArrangedSubviews:@[ tabSwitcher, omnibox, toolsMenu ]];
  toolbarItems.translatesAutoresizingMaskIntoConstraints = NO;
  toolbarItems.spacing = 16.0;
  toolbarItems.distribution = UIStackViewDistributionFillProportionally;
  [self.view addSubview:toolbarItems];
  [NSLayoutConstraint activateConstraints:@[
    [toolbarItems.leadingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.leadingAnchor],
    [toolbarItems.trailingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.trailingAnchor],
    [toolbarItems.bottomAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.bottomAnchor],
  ]];
}

#pragma mark - Public API

- (void)setCurrentPageText:(NSString*)text {
  self.omnibox.text = text;
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  return [view convertRect:self.toolsMenu.bounds fromView:self.toolsMenu];
}

#pragma mark - ToolsMenuActions

- (void)showToolsMenu:(id)sender {
  [self.actionDelegate showToolsMenu];
}

- (void)closeToolsMenu:(id)sender {
  [self.actionDelegate closeToolsMenu];
}

@end
