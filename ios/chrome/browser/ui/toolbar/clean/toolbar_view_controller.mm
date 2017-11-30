// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_component_options.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarViewController ()
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
@property(nonatomic, strong) ToolbarButtonUpdater* buttonUpdater;
@property(nonatomic, strong) UIView* locationBarContainer;
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, strong) ToolbarButton* backButton;
@property(nonatomic, strong) ToolbarButton* forwardButton;
@property(nonatomic, strong) ToolbarButton* tabSwitchStripButton;
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;
@property(nonatomic, strong) ToolbarButton* shareButton;
@property(nonatomic, strong) ToolbarButton* reloadButton;
@property(nonatomic, strong) ToolbarButton* stopButton;
@property(nonatomic, strong) ToolbarButton* voiceSearchButton;
@property(nonatomic, strong) ToolbarButton* bookmarkButton;
@property(nonatomic, assign) BOOL voiceSearchEnabled;
@property(nonatomic, strong) MDCProgressView* progressBar;
// Background view, used to display the incognito NTP background color on the
// toolbar.
@property(nonatomic, strong) UIView* backgroundView;
// Whether a page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
@end

@implementation ToolbarViewController
@synthesize backgroundView = _backgroundView;
@synthesize buttonFactory = _buttonFactory;
@synthesize buttonUpdater = _buttonUpdater;
@synthesize dispatcher = _dispatcher;
@synthesize locationBarView = _locationBarView;
@synthesize stackView = _stackView;
@synthesize loading = _loading;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize tabSwitchStripButton = _tabSwitchStripButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize shareButton = _shareButton;
@synthesize reloadButton = _reloadButton;
@synthesize stopButton = _stopButton;
@synthesize voiceSearchButton = _voiceSearchButton;
@synthesize bookmarkButton = _bookmarkButton;
@synthesize voiceSearchEnabled = _voiceSearchEnabled;
@synthesize progressBar = _progressBar;

#pragma mark - Properties accessor

- (UIView*)backgroundView {
  if (!_backgroundView) {
    _backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _backgroundView.backgroundColor =
        [UIColor colorWithWhite:kNTPBackgroundColorBrightnessIncognito
                          alpha:1.0];
    [self.view insertSubview:_backgroundView atIndex:0];
    AddSameConstraints(self.view, _backgroundView);
  }
  return _backgroundView;
}

#pragma mark - Public

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                     buttonFactory:(ToolbarButtonFactory*)buttonFactory
                     buttonUpdater:(ToolbarButtonUpdater*)buttonUpdater {
  _dispatcher = dispatcher;
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _buttonFactory = buttonFactory;
    _buttonUpdater = buttonUpdater;
    [self setUpToolbarButtons];
    [self setUpLocationBarContainer];
    [self setUpProgressBar];
  }
  return self;
}

- (void)contractOmnibox {
  // TODO(crbug.com/785210): Implement this.
}

- (void)expandOmniboxAnimated:(BOOL)animated {
  // TODO(crbug.com/785210): Implement this.
}

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
  // TODO(crbug.com/785756): Implement this.
}

- (void)resetAfterSideSwipeSnapshot {
  // TODO(crbug.com/785756): Implement this.
}

- (void)setBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha {
  self.backgroundView.alpha = alpha;
}

- (void)showPrerenderingAnimation {
  __weak ToolbarViewController* weakSelf = self;
  [self.progressBar setProgress:0];
  [self.progressBar setHidden:NO
                     animated:YES
                   completion:^(BOOL finished) {
                     [weakSelf stopProgressBar];
                   }];
}

#pragma mark - View lifecyle

- (void)viewDidLoad {
  // The view can be obstructed by the background view.
  self.view.backgroundColor =
      [self.buttonFactory.toolbarConfiguration backgroundColor];

  [self setUpToolbarStackView];
  if (self.locationBarView) {
    [self.locationBarContainer addSubview:self.locationBarView];
    AddSameConstraints(self.locationBarContainer, self.locationBarView);
  }
  [self.locationBarContainer addSubview:self.bookmarkButton];
  [self.locationBarContainer addSubview:self.voiceSearchButton];
  [self.view addSubview:self.stackView];
  [self.view addSubview:self.progressBar];
  [self setConstraints];
}

- (void)viewDidAppear:(BOOL)animated {
  [self updateAllButtonsVisibility];
  [super viewDidAppear:animated];
}

#pragma mark - View Setup

// Sets up the StackView that contains toolbar navigation items.
- (void)setUpToolbarStackView {
  self.stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, self.stopButton,
    self.locationBarContainer, self.shareButton, self.tabSwitchStripButton,
    self.toolsMenuButton
  ]];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.spacing = kStackViewSpacing;
  self.stackView.distribution = UIStackViewDistributionFill;
}

- (void)setConstraints {
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
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
    [self.bookmarkButton.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
    [self.voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
    [self.voiceSearchButton.trailingAnchor
        constraintEqualToAnchor:self.locationBarContainer.trailingAnchor],
    [self.bookmarkButton.trailingAnchor
        constraintEqualToAnchor:self.voiceSearchButton.leadingAnchor],
  ];

  // Constraint so Toolbar stackview never overlaps with the Status Bar.
  NSLayoutYAxisAnchor* topAnchor;
  if (@available(iOS 11, *)) {
    topAnchor = self.view.safeAreaLayoutGuide.topAnchor;
  } else {
    topAnchor = self.topLayoutGuide.topAnchor;
  }
  [self.stackView.topAnchor
      constraintGreaterThanOrEqualToAnchor:topAnchor
                                  constant:kVerticalMargin]
      .active = YES;
  [self.view.bottomAnchor constraintEqualToAnchor:topAnchor
                                         constant:kToolbarHeight]
      .active = YES;

  // Set the constraints priority to UILayoutPriorityDefaultHigh so these are
  // not broken when the views are hidden or the VC's view size is 0.
  [self activateConstraints:constraints
               withPriority:UILayoutPriorityDefaultHigh];
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
  [self.tabSwitchStripButton addTarget:self.dispatcher
                                action:@selector(displayTabSwitcher)
                      forControlEvents:UIControlEventTouchUpInside];

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
  self.shareButton.titleLabel.text = @"Share";
  [self.shareButton addTarget:self.dispatcher
                       action:@selector(sharePage)
             forControlEvents:UIControlEventTouchUpInside];

  // Reload button.
  self.reloadButton = [self.buttonFactory reloadToolbarButton];
  self.reloadButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.reloadButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.reloadButton addTarget:self.dispatcher
                        action:@selector(reload)
              forControlEvents:UIControlEventTouchUpInside];

  // Stop button.
  self.stopButton = [self.buttonFactory stopToolbarButton];
  self.stopButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.stopButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.stopButton addTarget:self.dispatcher
                      action:@selector(stopLoading)
            forControlEvents:UIControlEventTouchUpInside];

  // Voice Search button.
  self.voiceSearchButton = [self.buttonFactory voiceSearchButton];
  self.voiceSearchButton.visibilityMask =
      ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.voiceSearchButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  self.voiceSearchButton.enabled = NO;

  // Bookmark button.
  self.bookmarkButton = [self.buttonFactory bookmarkToolbarButton];
  self.bookmarkButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [buttonConstraints
      addObject:[self.bookmarkButton.widthAnchor
                    constraintEqualToConstant:kToolbarButtonWidth]];
  [self.bookmarkButton addTarget:self.dispatcher
                          action:@selector(bookmarkPage)
                forControlEvents:UIControlEventTouchUpInside];

  // Add buttons to button updater.
  self.buttonUpdater.backButton = self.backButton;
  self.buttonUpdater.forwardButton = self.forwardButton;
  self.buttonUpdater.voiceSearchButton = self.voiceSearchButton;

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

- (void)setLocationBarView:(UIView*)view {
  if (_locationBarView == view) {
    return;
  }
  view.translatesAutoresizingMaskIntoConstraints = NO;

  if ([self isViewLoaded]) {
    [_locationBarView removeFromSuperview];
    [self.locationBarContainer addSubview:view];
    AddSameConstraints(self.locationBarContainer, view);
  }
  _locationBarView = view;
}

#pragma mark - Trait Collection Changes

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.horizontalSizeClass !=
      previousTraitCollection.horizontalSizeClass) {
    [self updateAllButtonsVisibility];
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

- (void)setLoadingState:(BOOL)loading {
  _loading = loading;
  self.reloadButton.hiddenInCurrentState = loading;
  self.stopButton.hiddenInCurrentState = !loading;
  if (!loading) {
    [self stopProgressBar];
  } else {
    [self.progressBar setProgress:0];
    [self.progressBar setHidden:NO animated:YES completion:nil];
  }
}

- (void)setLoadingProgressFraction:(double)progress {
  [self.progressBar setProgress:progress animated:YES completion:nil];
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

- (void)setPageBookmarked:(BOOL)bookmarked {
  self.bookmarkButton.selected = bookmarked;
}

- (void)setVoiceSearchEnabled:(BOOL)voiceSearchEnabled {
  _voiceSearchEnabled = voiceSearchEnabled;
  if (voiceSearchEnabled) {
    self.voiceSearchButton.enabled = YES;
    [self.voiceSearchButton addTarget:self.dispatcher
                               action:@selector(preloadVoiceSearch)
                     forControlEvents:UIControlEventTouchDown];
    [self.voiceSearchButton addTarget:self
                               action:@selector(startVoiceSearch:)
                     forControlEvents:UIControlEventTouchUpInside];
  }
}

- (void)setShareMenuEnabled:(BOOL)enabled {
  self.shareButton.enabled = enabled;
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return self.shareButton;
}

#pragma mark = BubbleViewAnchorPointProvider

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(self.tabSwitchStripButton.frame, direction);
  return [self.tabSwitchStripButton.superview
      convertPoint:anchorPoint
            toView:self.tabSwitchStripButton.window];
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(self.toolsMenuButton.frame, direction);
  return
      [self.toolsMenuButton.superview convertPoint:anchorPoint
                                            toView:self.toolsMenuButton.window];
}

#pragma mark - Helper Methods

// Updates all Buttons visibility to match any recent WebState change.
- (void)updateAllButtonsVisibility {
  for (UIView* view in self.stackView.arrangedSubviews) {
    if ([view isKindOfClass:[ToolbarButton class]]) {
      ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
      [button updateHiddenInCurrentSizeClass];
    }
  }
  [self.bookmarkButton updateHiddenInCurrentSizeClass];
  [self.voiceSearchButton updateHiddenInCurrentSizeClass];
}

// Sets the priority for an array of constraints and activates them.
- (void)activateConstraints:(NSArray*)constraintsArray
               withPriority:(UILayoutPriority)priority {
  for (NSLayoutConstraint* constraint in constraintsArray) {
    constraint.priority = priority;
  }
  [NSLayoutConstraint activateConstraints:constraintsArray];
}

#pragma mark - Private

// Sets the progress of the progressBar to 1 then hides it.
- (void)stopProgressBar {
  __weak MDCProgressView* weakProgressBar = self.progressBar;
  __weak ToolbarViewController* weakSelf = self;
  [self.progressBar
      setProgress:1
         animated:YES
       completion:^(BOOL finished) {
         if (!weakSelf.loading) {
           [weakProgressBar setHidden:YES animated:YES completion:nil];
         }
       }];
}

// TODO(crbug.com/789104): Use named layout guide instead of passing the view.
// Target of the voice search button.
- (void)startVoiceSearch:(id)sender {
  UIView* view = base::mac::ObjCCastStrict<UIView>(sender);
  StartVoiceSearchCommand* command =
      [[StartVoiceSearchCommand alloc] initWithOriginView:view];
  [self.dispatcher startVoiceSearch:command];
}

@end
