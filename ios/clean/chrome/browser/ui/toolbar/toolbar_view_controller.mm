// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tools_menu_actions.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button+factory.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_component_options.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Stackview Vertical Margin.
CGFloat kVerticalMargin = 5.0f;
// Stackview Horizontal Margin.
CGFloat kHorizontalMargin = 8.0f;
}  // namespace

@interface ToolbarViewController ()<ToolsMenuActions>
@property(nonatomic, strong) UITextField* omnibox;
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, strong) ToolbarButton* backButton;
@property(nonatomic, strong) ToolbarButton* forwardButton;
@property(nonatomic, strong) ToolbarButton* tabSwitchStripButton;
@property(nonatomic, strong) ToolbarButton* tabSwitchGridButton;
@property(nonatomic, strong) ToolbarButton* toolsMenuButton;
@property(nonatomic, strong) ToolbarButton* shareButton;
@property(nonatomic, strong) ToolbarButton* reloadButton;
@property(nonatomic, strong) ToolbarButton* stopButton;
@end

@implementation ToolbarViewController
@synthesize dispatcher = _dispatcher;
@synthesize stackView = _stackView;
@synthesize omnibox = _omnibox;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize tabSwitchStripButton = _tabSwitchStripButton;
@synthesize tabSwitchGridButton = _tabSwitchGridButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize shareButton = _shareButton;
@synthesize reloadButton = _reloadButton;
@synthesize stopButton = _stopButton;

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setUpToolbarButtons];
    // PLACEHOLDER: Omnibox could have some "Toolbar component" traits if
    // needed.
    UITextField* omnibox = [[UITextField alloc] initWithFrame:CGRectZero];
    omnibox.translatesAutoresizingMaskIntoConstraints = NO;
    omnibox.backgroundColor = [UIColor whiteColor];
    omnibox.enabled = NO;
    self.omnibox = omnibox;
  }
  return self;
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor lightGrayColor];

  // Stack view to contain toolbar items.
  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, self.stopButton,
    self.omnibox, self.shareButton, self.tabSwitchStripButton,
    self.tabSwitchGridButton, self.toolsMenuButton
  ]];
  [self updateAllButtonsVisibility];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.spacing = 16.0;
  self.stackView.distribution = UIStackViewDistributionFillProportionally;
  [self.view addSubview:self.stackView];

  // Set constraints.
  [self.view setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                 UIViewAutoresizingFlexibleHeight];
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                             constant:kVerticalMargin],
    [self.stackView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor
                                                constant:-kVerticalMargin],
    [self.stackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kHorizontalMargin],
    [self.stackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kHorizontalMargin],
  ]];
}

#pragma mark - Components Setup

- (void)setUpToolbarButtons {
  // Back button.
  self.backButton = [ToolbarButton backToolbarButton];
  self.backButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                   ToolbarComponentVisibilityRegularWidth;
  [self.backButton addTarget:self
                      action:@selector(goBack:)
            forControlEvents:UIControlEventTouchUpInside];

  // Forward button.
  self.forwardButton = [ToolbarButton forwardToolbarButton];
  self.forwardButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                      ToolbarComponentVisibilityRegularWidth;
  [self.forwardButton addTarget:self
                         action:@selector(goForward:)
               forControlEvents:UIControlEventTouchUpInside];

  // Tab switcher Strip button.
  self.tabSwitchStripButton = [ToolbarButton tabSwitcherStripToolbarButton];
  self.tabSwitchStripButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidth |
      ToolbarComponentVisibilityRegularWidth;
  [self.tabSwitchStripButton addTarget:nil
                                action:@selector(showTabStrip:)
                      forControlEvents:UIControlEventTouchUpInside];

  // Tab switcher Grid button.
  self.tabSwitchGridButton = [ToolbarButton tabSwitcherGridToolbarButton];
  self.tabSwitchGridButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidth |
      ToolbarComponentVisibilityRegularWidth;
  [self.tabSwitchGridButton addTarget:nil
                               action:@selector(showTabGrid:)
                     forControlEvents:UIControlEventTouchUpInside];
  self.tabSwitchGridButton.hiddenInCurrentState = YES;

  // Tools menu button.
  self.toolsMenuButton = [ToolbarButton toolsMenuToolbarButton];
  self.toolsMenuButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                        ToolbarComponentVisibilityRegularWidth;
  [self.toolsMenuButton addTarget:nil
                           action:@selector(showToolsMenu:)
                 forControlEvents:UIControlEventTouchUpInside];

  // Share button.
  self.shareButton = [ToolbarButton shareToolbarButton];
  self.shareButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self.shareButton addTarget:nil
                       action:@selector(showShareMenu:)
             forControlEvents:UIControlEventTouchUpInside];

  // Reload button.
  self.reloadButton = [ToolbarButton reloadToolbarButton];
  self.reloadButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self.reloadButton addTarget:self
                        action:@selector(reload:)
              forControlEvents:UIControlEventTouchUpInside];

  // Stop button.
  self.stopButton = [ToolbarButton stopToolbarButton];
  self.stopButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self.stopButton addTarget:self
                      action:@selector(stop:)
            forControlEvents:UIControlEventTouchUpInside];
}

#pragma mark - Trait Collection Changes

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.horizontalSizeClass !=
      previousTraitCollection.horizontalSizeClass) {
    for (UIView* view in self.stackView.arrangedSubviews) {
      if ([view isKindOfClass:[ToolbarButton class]]) {
        ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
        [button updateHiddenInCurrentSizeClass];
      }
    }
  }
}

#pragma mark - ToolbarWebStateConsumer

- (void)setCurrentPageText:(NSString*)text {
  self.omnibox.text = text;
}

- (void)setCanGoForward:(BOOL)canGoForward {
  self.forwardButton.enabled = canGoForward;
}

- (void)setCanGoBack:(BOOL)canGoBack {
  self.backButton.enabled = canGoBack;
}

- (void)setIsLoading:(BOOL)isLoading {
  self.reloadButton.hiddenInCurrentState = isLoading;
  self.stopButton.hiddenInCurrentState = !isLoading;
  [self updateAllButtonsVisibility];
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  return [view convertRect:self.toolsMenuButton.bounds
                  fromView:self.toolsMenuButton];
}

#pragma mark - ToolsMenuActions

- (void)showToolsMenu:(id)sender {
  [self.dispatcher showToolsMenu];
}

- (void)closeToolsMenu:(id)sender {
  [self.dispatcher closeToolsMenu];
}

#pragma mark - NavigationActions

- (void)goBack:(id)sender {
  [self.dispatcher goBack];
}

- (void)goForward:(id)sender {
  [self.dispatcher goForward];
}

- (void)stop:(id)sender {
  [self.dispatcher stopLoadingPage];
}

- (void)reload:(id)sender {
  [self.dispatcher reloadPage];
}

#pragma mark - TabStripEvents

- (void)tabStripDidShow:(id)sender {
  self.tabSwitchStripButton.hiddenInCurrentState = YES;
  self.tabSwitchGridButton.hiddenInCurrentState = NO;
  [self updateAllButtonsVisibility];
}

- (void)tabStripDidHide:(id)sender {
  self.tabSwitchStripButton.hiddenInCurrentState = NO;
  self.tabSwitchGridButton.hiddenInCurrentState = YES;
  [self updateAllButtonsVisibility];
}

#pragma mark - Helper Methods

// PLACEHOLDER: We are not sure yet how WebState changes will affect Toolbar
// Buttons, but the VC will eventually set the flag on the ToolbarButton
// indicating it could or not it should be hidden. Once this is done we will
// update all the ToolbarButtons visibility.
// Updates all Buttons visibility to match any recent WebState change.
- (void)updateAllButtonsVisibility {
  for (UIView* view in self.stackView.arrangedSubviews) {
    if ([view isKindOfClass:[ToolbarButton class]]) {
      ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
      [button setHiddenForCurrentStateAndSizeClass];
    }
  }
}

@end
