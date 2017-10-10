// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_strip_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button_factory.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_component_options.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_configuration.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_constants.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarViewController ()
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
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
@property(nonatomic, strong) MDCProgressView* progressBar;
@end

@implementation ToolbarViewController
@synthesize buttonFactory = _buttonFactory;
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
@synthesize progressBar = _progressBar;
@synthesize usesTabStrip = _usesTabStrip;

- (instancetype)initWithDispatcher:(id<NavigationCommands,
                                       TabGridCommands,
                                       TabHistoryPopupCommands,
                                       TabStripCommands,
                                       ToolsMenuCommands>)dispatcher
                     buttonFactory:(ToolbarButtonFactory*)buttonFactory {
  _dispatcher = dispatcher;
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _buttonFactory = buttonFactory;
    [self setUpToolbarButtons];
    [self setUpLocationBarContainer];
    [self setUpProgressBar];
  }
  return self;
}

#pragma mark - View lifecyle

- (void)viewDidLoad {
  self.view.backgroundColor =
      [self.buttonFactory.toolbarConfiguration backgroundColor];
  [self addChildViewController:self.locationBarViewController
                     toSubview:self.locationBarContainer];
  [self setUpToolbarStackView];
  [self.view addSubview:self.stackView];
  [self.view addSubview:self.progressBar];
  [self setConstraints];
}

#pragma mark - View Setup

// Sets up the StackView that contains toolbar navigation items.
- (void)setUpToolbarStackView {
  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, self.stopButton,
    self.locationBarContainer, self.shareButton, self.tabSwitchStripButton,
    self.tabSwitchGridButton, self.toolsMenuButton
  ]];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.spacing = kStackViewSpacing;
  self.stackView.distribution = UIStackViewDistributionFill;
  [self updateAllButtonsVisibility];
}

- (void)setConstraints {
  [self.view setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                 UIViewAutoresizingFlexibleHeight];
  NSArray* constraints = @[
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
    [self.progressBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.progressBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.progressBar.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.progressBar.heightAnchor
        constraintEqualToConstant:kProgressBarHeight],
  ];

  // Constraint so Toolbar stackview never overlaps with the Status Bar.
  NSLayoutConstraint* constraintTop = [self.stackView.topAnchor
      constraintGreaterThanOrEqualToAnchor:self.topLayoutGuide.bottomAnchor
                                  constant:kVerticalMargin];
  constraintTop.priority = UILayoutPriorityRequired;
  constraintTop.active = YES;

  // Set the constraints priority to UILayoutPriorityDefaultHigh so these are
  // not broken when the views are hidden or the VC's view size is 0.
  [self activateConstraints:constraints
               withPriority:UILayoutPriorityDefaultHigh];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  // We need to dismiss the ToolsMenu every time the Toolbar frame changes
  // (e.g. Size changes, rotation changes, etc.)
  [self.dispatcher closeToolsMenu];
}

#pragma mark - Components Setup

- (void)setUpToolbarButtons {
  NSMutableArray* buttonConstraints = [[NSMutableArray alloc] init];

  // Back button.
  self.backButton = [self.buttonFactory backToolbarButton];
  self.backButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                   ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.backButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.backButton addTarget:self.dispatcher
                      action:@selector(goBack)
            forControlEvents:UIControlEventTouchUpInside];
  UILongPressGestureRecognizer* backHistoryLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [self.backButton addGestureRecognizer:backHistoryLongPress];

  // Forward button.
  self.forwardButton = [self.buttonFactory forwardToolbarButton];
  self.forwardButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidthOnlyWhenEnabled |
      ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.forwardButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.forwardButton addTarget:self.dispatcher
                         action:@selector(goForward)
               forControlEvents:UIControlEventTouchUpInside];
  UILongPressGestureRecognizer* forwardHistoryLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [self.forwardButton addGestureRecognizer:forwardHistoryLongPress];

  // Tab switcher Strip button.
  self.tabSwitchStripButton =
      [self.buttonFactory tabSwitcherStripToolbarButton];
  self.tabSwitchStripButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidth |
      ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.tabSwitchStripButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.tabSwitchStripButton addTarget:self
                                action:@selector(tabSwitcherButtonTapped:)
                      forControlEvents:UIControlEventTouchUpInside];

  // Tab switcher Grid button.
  self.tabSwitchGridButton = [self.buttonFactory tabSwitcherGridToolbarButton];
  self.tabSwitchGridButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidth |
      ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.tabSwitchGridButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.tabSwitchGridButton addTarget:self.dispatcher
                               action:@selector(showTabGrid)
                     forControlEvents:UIControlEventTouchUpInside];
  self.tabSwitchGridButton.hiddenInCurrentState = YES;

  // Tools menu button.
  self.toolsMenuButton = [self.buttonFactory toolsMenuToolbarButton];
  self.toolsMenuButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                        ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.toolsMenuButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.toolsMenuButton addTarget:self.dispatcher
                           action:@selector(showToolsMenu)
                 forControlEvents:UIControlEventTouchUpInside];

  // Share button.
  self.shareButton = [self.buttonFactory shareToolbarButton];
  self.shareButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.shareButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  // TODO(crbug.com/740793): Remove alert once share is implemented.
  self.shareButton.titleLabel.text = @"Share";
  [self.shareButton addTarget:self
                       action:@selector(showAlert:)
             forControlEvents:UIControlEventTouchUpInside];

  // Reload button.
  self.reloadButton = [self.buttonFactory reloadToolbarButton];
  self.reloadButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.reloadButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.reloadButton addTarget:self.dispatcher
                        action:@selector(reloadPage)
              forControlEvents:UIControlEventTouchUpInside];

  // Stop button.
  self.stopButton = [self.buttonFactory stopToolbarButton];
  self.stopButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.stopButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.stopButton addTarget:self.dispatcher
                      action:@selector(stopLoadingPage)
            forControlEvents:UIControlEventTouchUpInside];

  // Set the button constraint priority to UILayoutPriorityDefaultHigh so
  // these are not broken when being hidden by the StackView.
  [self activateConstraints:buttonConstraints
               withPriority:UILayoutPriorityDefaultHigh];
}

- (void)setUpLocationBarContainer {
  UIView* locationBarContainer = [[UIView alloc] initWithFrame:CGRectZero];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration omniboxBackgroundColor];
  locationBarContainer.layer.borderWidth = kLocationBarBorderWidth;
  locationBarContainer.layer.borderColor =
      [self.buttonFactory.toolbarConfiguration omniboxBorderColor].CGColor;
  locationBarContainer.layer.shadowRadius = kLocationBarShadowRadius;
  locationBarContainer.layer.shadowOpacity = kLocationBarShadowOpacity;
  locationBarContainer.layer.shadowOffset = CGSizeMake(0.0f, 0.5f);

  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer = locationBarContainer;
}

- (void)setUpProgressBar {
  MDCProgressView* progressBar = [[MDCProgressView alloc] init];
  progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  progressBar.hidden = YES;
  self.progressBar = progressBar;
}

#pragma mark - Button Actions

- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  if (gesture.view == self.backButton) {
    [self.dispatcher showTabHistoryPopupForBackwardHistory];
  } else if (gesture.view == self.forwardButton) {
    [self.dispatcher showTabHistoryPopupForForwardHistory];
  }
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
  [self.progressBar setHidden:!isLoading animated:YES completion:nil];
  [self updateAllButtonsVisibility];
}

- (void)setLoadingProgressFraction:(double)progress {
  [self.progressBar setProgress:progress animated:YES completion:nil];
}

- (void)setTabStripVisible:(BOOL)visible {
  self.tabSwitchStripButton.hiddenInCurrentState = visible;
  self.tabSwitchGridButton.hiddenInCurrentState = !visible;
  [self updateAllButtonsVisibility];
}

- (void)setTabCount:(int)tabCount {
  // Return if tabSwitchStripButton wasn't initialized.
  if (!self.tabSwitchStripButton)
    return;

  // Update the text shown in the |self.tabSwitchStripButton|. Note that the
  // button's title may be empty or contain an easter egg, but the accessibility
  // value will always be equal to |tabCount|.
  NSString* tabStripButtonValue = [NSString stringWithFormat:@"%d", tabCount];
  NSString* tabStripButtonTitle;
  if (tabCount <= 0) {
    tabStripButtonTitle = @"";
  } else if (tabCount > kShowTabStripButtonMaxTabCount) {
    // As an easter egg, show a smiley face instead of the count if the user has
    // more than 99 tabs open.
    tabStripButtonTitle = @":)";
    [[self.tabSwitchStripButton titleLabel]
        setFont:[UIFont boldSystemFontOfSize:kFontSizeFewerThanTenTabs]];
  } else {
    tabStripButtonTitle = tabStripButtonValue;
    if (tabCount < 10) {
      [[self.tabSwitchStripButton titleLabel]
          setFont:[UIFont boldSystemFontOfSize:kFontSizeFewerThanTenTabs]];
    } else {
      [[self.tabSwitchStripButton titleLabel]
          setFont:[UIFont boldSystemFontOfSize:kFontSizeTenTabsOrMore]];
    }
  }

  [self.tabSwitchStripButton setTitle:tabStripButtonTitle
                             forState:UIControlStateNormal];
  [self.tabSwitchStripButton setAccessibilityValue:tabStripButtonValue];
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  return [view convertRect:self.toolsMenuButton.bounds
                  fromView:self.toolsMenuButton];
}

#pragma mark - TabHistoryPositioner

- (CGPoint)originPointForToolbarButton:(ToolbarButtonType)toolbarButton {
  UIButton* historyButton =
      (toolbarButton == ToolbarButtonTypeBack) ? _backButton : _forwardButton;

  // Set the origin for the tools popup to the leading side of the bottom of the
  // pressed buttons.
  CGRect buttonBounds = [historyButton.imageView bounds];
  CGPoint leadingBottomCorner = CGPointMake(CGRectGetLeadingEdge(buttonBounds),
                                            CGRectGetMaxY(buttonBounds));
  CGPoint origin = [historyButton.imageView convertPoint:leadingBottomCorner
                                                  toView:historyButton.window];
  return origin;
}

#pragma mark - TabHistoryUIUpdater

- (void)updateUIForTabHistoryPresentationFrom:(ToolbarButtonType)button {
  ToolbarButton* historyButton = button ? self.backButton : self.forwardButton;
  historyButton.selected = YES;
}

- (void)updateUIForTabHistoryWasDismissed {
  self.backButton.selected = NO;
  self.forwardButton.selected = NO;
}

#pragma mark - TabHistoryPresentation

- (UIView*)viewForTabHistoryPresentation {
  return self.parentViewController.view;
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

// Sets the priority for an array of constraints and activates them.
- (void)activateConstraints:(NSArray*)constraintsArray
               withPriority:(UILayoutPriority)priority {
  for (NSLayoutConstraint* constraint in constraintsArray) {
    constraint.priority = priority;
  }
  [NSLayoutConstraint activateConstraints:constraintsArray];
}

// TODO(crbug.com/740793): Remove this method once no item is using it.
- (void)showAlert:(UIButton*)sender {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:sender.titleLabel.text
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.parentViewController presentViewController:alertController
                                          animated:YES
                                        completion:nil];
}

#pragma mark - Button actions

// The action performed depends on the experimental setting of using the tab
// strip.
- (void)tabSwitcherButtonTapped:(id)sender {
  self.usesTabStrip ? [self.dispatcher showTabStrip]
                    : [self.dispatcher showTabGrid];
}

@end
