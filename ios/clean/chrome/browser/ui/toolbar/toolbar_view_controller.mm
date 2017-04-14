// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
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

@interface ToolbarViewController ()
@property(nonatomic, strong) UIView* locationBarContainer;
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
@synthesize locationBarViewController = _locationBarViewController;
@synthesize stackView = _stackView;
@synthesize locationBarContainer = _locationBarContainer;
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
    [self setUpLocationBarContainer];
  }
  return self;
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor lightGrayColor];

  [self addChildViewController:self.locationBarViewController
                     toSubview:self.locationBarContainer];

  // Stack view to contain toolbar items.
  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, self.stopButton,
    self.locationBarContainer, self.shareButton, self.tabSwitchStripButton,
    self.tabSwitchGridButton, self.toolsMenuButton
  ]];
  [self updateAllButtonsVisibility];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.spacing = 16.0;
  self.stackView.distribution = UIStackViewDistributionFill;
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
  self.forwardButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidthOnlyWhenEnabled |
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
  [self.tabSwitchGridButton addTarget:self
                               action:@selector(showTabGrid:)
                     forControlEvents:UIControlEventTouchUpInside];
  self.tabSwitchGridButton.hiddenInCurrentState = YES;

  // Tools menu button.
  self.toolsMenuButton = [ToolbarButton toolsMenuToolbarButton];
  self.toolsMenuButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                        ToolbarComponentVisibilityRegularWidth;
  [self.toolsMenuButton addTarget:self
                           action:@selector(showToolsMenu:)
                 forControlEvents:UIControlEventTouchUpInside];

  // Share button.
  self.shareButton = [ToolbarButton shareToolbarButton];
  self.shareButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self.shareButton addTarget:self
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

- (void)setUpLocationBarContainer {
  UIView* locationBarContainer = [[UIView alloc] initWithFrame:CGRectZero];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarContainer.backgroundColor = [UIColor whiteColor];
  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer = locationBarContainer;
}

#pragma mark - View Controller Containment

- (void)addChildViewController:(UIViewController*)viewController
                     toSubview:(UIView*)subview {
  if (!viewController || !subview) {
    return;
  }
  [self addChildViewController:viewController];
  viewController.view.translatesAutoresizingMaskIntoConstraints = YES;
  viewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  viewController.view.frame = subview.bounds;
  [subview addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
}

- (void)setLocationBarViewController:(UIViewController*)controller {
  if (self.locationBarViewController == controller) {
    return;
  }

  if ([self isViewLoaded]) {
    // Remove the old child view controller.
    if (self.locationBarViewController) {
      DCHECK_EQ(self, self.locationBarViewController.parentViewController);
      [self.locationBarViewController willMoveToParentViewController:nil];
      [self.locationBarViewController.view removeFromSuperview];
      [self.locationBarViewController removeFromParentViewController];
    }
    // Add the new child view controller.
    [self addChildViewController:controller
                       toSubview:self.locationBarContainer];
  }
  _locationBarViewController = controller;
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
}

- (void)setCanGoForward:(BOOL)canGoForward {
  self.forwardButton.enabled = canGoForward;
  // Update the visibility since the Forward button will be hidden on
  // CompactWidth when disabled.
  [self.forwardButton updateHiddenInCurrentSizeClass];
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

#pragma mark - Private Methods

- (void)showToolsMenu:(id)sender {
  [self.dispatcher showToolsMenu];
}

- (void)closeToolsMenu:(id)sender {
  [self.dispatcher closeToolsMenu];
}

- (void)showShareMenu:(id)sender {
  [self.dispatcher showShareMenu];
}

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

- (void)showTabGrid:(id)sender {
  [self.dispatcher showTabGrid];
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
