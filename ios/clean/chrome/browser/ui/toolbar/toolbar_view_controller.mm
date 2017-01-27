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

namespace {
// Stackview Vertical Margin.
CGFloat kVerticalMargin = 5.0f;
}  // namespace

@interface ToolbarViewController ()<ToolsMenuActions>
@property(nonatomic, weak) UITextField* omnibox;
@property(nonatomic, strong) UIButton* backButton;
@property(nonatomic, strong) UIButton* forwardButton;
@property(nonatomic, strong) UIButton* tabSwitcherButton;
@property(nonatomic, strong) UIButton* toolsMenuButton;
@property(nonatomic, strong) UIButton* shareButton;
@property(nonatomic, strong) UIButton* reloadButton;
@end

@implementation ToolbarViewController
@synthesize toolbarCommandHandler = _toolbarCommandHandler;
@synthesize omnibox = _omnibox;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize tabSwitcherButton = _tabSwitcherButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize shareButton = _shareButton;
@synthesize reloadButton = _reloadButton;

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor lightGrayColor];

  [self setUpToolbarButtons];
  [self setUpRegularWidthToolbarButtons];

  // Placeholder omnibox.
  UITextField* omnibox = [[UITextField alloc] initWithFrame:CGRectZero];
  omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  omnibox.backgroundColor = [UIColor whiteColor];
  omnibox.enabled = NO;
  self.omnibox = omnibox;

  // Stack view to contain toolbar items.
  UIStackView* toolbarItems = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, omnibox,
    self.shareButton, self.tabSwitcherButton, self.toolsMenuButton
  ]];
  [self hideButtonsForSize:self.view.bounds.size];
  toolbarItems.translatesAutoresizingMaskIntoConstraints = NO;
  toolbarItems.spacing = 16.0;
  toolbarItems.distribution = UIStackViewDistributionFillProportionally;
  [self.view addSubview:toolbarItems];

  // Set constraints.
  [NSLayoutConstraint activateConstraints:@[
    [toolbarItems.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                           constant:kVerticalMargin],
    [toolbarItems.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor
                                              constant:-kVerticalMargin],
    [toolbarItems.leadingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.leadingAnchor],
    [toolbarItems.trailingAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.trailingAnchor],
  ]];
}

#pragma mark - UIContentContainer

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self hideButtonsForSize:size];
}

#pragma mark - View Setup

// Hide buttons for different Size Classes.
- (void)hideButtonsForSize:(CGSize)size {
  // PLACEHOLDER: Create a method that checks the Class Size using CGSize and
  // device as parameters. This could take place in a subclass of UIButton.
  if (size.width <= 670.0) {
    self.shareButton.hidden = YES;
    self.reloadButton.hidden = YES;
  } else {
    self.shareButton.hidden = NO;
    self.reloadButton.hidden = NO;
  }
}

- (void)setUpToolbarButtons {
  // Back button
  self.backButton = [self
      buttonWithImageForNormalState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_BACK, YES)
                   highlightedState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_BACK_PRESSED, YES)
                      disabledState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_BACK_DISABLED,
                                        YES)
                             action:@selector(goBack:)];

  // Forward button
  self.forwardButton = [self
      buttonWithImageForNormalState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_FORWARD, YES)
                   highlightedState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_FORWARD_PRESSED,
                                        YES)
                      disabledState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_FORWARD_DISABLED,
                                        YES)
                             action:@selector(goForward:)];

  // Tab switcher button.
  self.tabSwitcherButton =
      [self buttonWithImageForNormalState:
                [UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
                         highlightedState:nil
                            disabledState:nil
                                   action:@selector(toggleTabStrip:)];

  // Tools menu button.
  self.toolsMenuButton = [self
      buttonWithImageForNormalState:[UIImage imageNamed:@"tabswitcher_menu"]
                   highlightedState:nil
                      disabledState:nil
                             action:@selector(showToolsMenu:)];
  [self.toolsMenuButton
      setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
}

- (void)setUpRegularWidthToolbarButtons {
  // Share button.
  self.shareButton = [self
      buttonWithImageForNormalState:NativeImage(IDR_IOS_TOOLBAR_LIGHT_SHARE)
                   highlightedState:NativeImage(
                                        IDR_IOS_TOOLBAR_LIGHT_SHARE_PRESSED)
                      disabledState:NativeImage(
                                        IDR_IOS_TOOLBAR_LIGHT_SHARE_DISABLED)
                             action:@selector(showShareMenu:)];

  // Reload button.
  self.reloadButton = [self
      buttonWithImageForNormalState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_RELOAD, YES)
                   highlightedState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_RELOAD_PRESSED,
                                        YES)
                      disabledState:NativeReversableImage(
                                        IDR_IOS_TOOLBAR_LIGHT_RELOAD_DISABLED,
                                        YES)
                             action:@selector(reload:)];
}

#pragma mark - Public API

- (void)setCurrentPageText:(NSString*)text {
  self.omnibox.text = text;
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  return [view convertRect:self.toolsMenuButton.bounds
                  fromView:self.toolsMenuButton];
}

#pragma mark - ToolsMenuActions

- (void)showToolsMenu:(id)sender {
  [self.toolbarCommandHandler showToolsMenu];
}

- (void)closeToolsMenu:(id)sender {
  [self.toolbarCommandHandler closeToolsMenu];
}

#pragma mark - Helper Methods
// Constructor for a Toolbar button.
- (UIButton*)buttonWithImageForNormalState:(UIImage*)normalImage
                          highlightedState:(UIImage*)highlightedImage
                             disabledState:(UIImage*)disabledImage
                                    action:(SEL)actionSelector {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setImage:normalImage forState:UIControlStateNormal];
  [button setImage:highlightedImage forState:UIControlStateHighlighted];
  [button setImage:disabledImage forState:UIControlStateDisabled];
  [button addTarget:nil
                action:actionSelector
      forControlEvents:UIControlEventTouchUpInside];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];
  return button;
}

@end
