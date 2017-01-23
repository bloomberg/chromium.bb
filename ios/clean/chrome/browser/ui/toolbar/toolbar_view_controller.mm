// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#import "ios/clean/chrome/browser/ui/actions/navigation_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tools_menu_actions.h"
#import "ios/clean/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarViewController ()<ToolsMenuActions>
@property(nonatomic, weak) UITextField* omnibox;
@property(nonatomic, weak) UIButton* toolsMenu;
@end

@implementation ToolbarViewController
@synthesize toolbarCommandHandler = _toolbarCommandHandler;
@synthesize omnibox = _omnibox;
@synthesize toolsMenu = _toolsMenu;

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor lightGrayColor];

  // Navigation buttons
  UIButton* backButton = [UIButton buttonWithType:UIButtonTypeSystem];
  backButton.translatesAutoresizingMaskIntoConstraints = NO;
  [backButton setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK, YES)
              forState:UIControlStateNormal];
  [backButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK_PRESSED, YES)
      forState:UIControlStateHighlighted];
  [backButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK_DISABLED, YES)
      forState:UIControlStateDisabled];
  [backButton addTarget:nil
                 action:@selector(goBack:)
       forControlEvents:UIControlEventTouchUpInside];

  UIButton* forwardButton = [UIButton buttonWithType:UIButtonTypeSystem];
  forwardButton.translatesAutoresizingMaskIntoConstraints = NO;
  [forwardButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_FORWARD, YES)
      forState:UIControlStateNormal];
  [forwardButton
      setImage:NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_FORWARD_PRESSED, YES)
      forState:UIControlStateHighlighted];
  [forwardButton setImage:NativeReversableImage(
                              IDR_IOS_TOOLBAR_LIGHT_FORWARD_DISABLED, YES)
                 forState:UIControlStateDisabled];
  [forwardButton addTarget:nil
                    action:@selector(goForward:)
          forControlEvents:UIControlEventTouchUpInside];

  // Tab switcher button.
  UIButton* tabSwitcher = [UIButton buttonWithType:UIButtonTypeSystem];
  tabSwitcher.translatesAutoresizingMaskIntoConstraints = NO;
  [tabSwitcher setImage:[UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
               forState:UIControlStateNormal];
  [tabSwitcher addTarget:nil
                  action:@selector(toggleTabStrip:)
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
  [toolsMenu setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
  [toolsMenu
      setImage:[[UIImage imageNamed:@"tabswitcher_menu"]
                   imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
      forState:UIControlStateNormal];
  [toolsMenu addTarget:nil
                action:@selector(showToolsMenu:)
      forControlEvents:UIControlEventTouchUpInside];
  self.toolsMenu = toolsMenu;

  // Stack view to contain toolbar items.
  UIStackView* toolbarItems = [[UIStackView alloc] initWithArrangedSubviews:@[
    backButton, forwardButton, omnibox, tabSwitcher, toolsMenu
  ]];
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
  [self.toolbarCommandHandler showToolsMenu];
}

- (void)closeToolsMenu:(id)sender {
  [self.toolbarCommandHandler closeToolsMenu];
}

@end
