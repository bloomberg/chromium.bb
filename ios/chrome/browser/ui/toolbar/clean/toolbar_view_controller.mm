// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/clean/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_component_options.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarViewController ()<ToolbarViewFullscreenDelegate>
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
@property(nonatomic, strong) ToolbarButtonUpdater* buttonUpdater;

// The main objects in the view. Positionned as
// [leadingStackView][locationBarContainer][trailingStackView]. The stack views
// contain the buttons which should be shown on each sides of the location bar.
@property(nonatomic, strong) UIStackView* leadingStackView;
@property(nonatomic, strong) UIView* locationBarContainer;
@property(nonatomic, strong) UIStackView* trailingStackView;

// Array containing all the |_leadingStackView| buttons, lazily instantiated.
@property(nonatomic, strong) NSArray<ToolbarButton*>* leadingStackViewButtons;
// Array containing all the |_trailingStackView| buttons, lazily instantiated.
@property(nonatomic, strong) NSArray<ToolbarButton*>* trailingStackViewButtons;
@property(nonatomic, strong) ToolbarButton* backButton;
@property(nonatomic, strong) ToolbarButton* forwardButton;
@property(nonatomic, strong) ToolbarButton* tabSwitchStripButton;
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;
@property(nonatomic, strong) ToolbarButton* shareButton;
@property(nonatomic, strong) ToolbarButton* reloadButton;
@property(nonatomic, strong) ToolbarButton* stopButton;
@property(nonatomic, strong) ToolbarButton* voiceSearchButton;
@property(nonatomic, strong) ToolbarButton* bookmarkButton;
@property(nonatomic, strong) ToolbarButton* contractButton;
@property(nonatomic, strong) ToolbarButton* locationBarLeadingButton;
@property(nonatomic, assign) BOOL voiceSearchEnabled;
@property(nonatomic, strong) MDCProgressView* progressBar;
@property(nonatomic, strong) UIStackView* locationBarContainerStackView;
// The shadow below the toolbar when the omnibox is contracted. Lazily
// instantiated.
@property(nonatomic, strong) UIImageView* shadowView;
// The shadow below the expanded omnibox. Lazily instantiated.
@property(nonatomic, strong) UIImageView* fullBleedShadowView;
// The shadow below the contracted location bar.
@property(nonatomic, strong) UIImageView* locationBarShadow;
// Background view, used to display the incognito NTP background color on the
// toolbar.
@property(nonatomic, strong) UIView* backgroundView;
// Whether a page is loading.
@property(nonatomic, assign, getter=isLoading) BOOL loading;
// Constraints used for the regular/contracted Toolbar state that will be
// deactivated and replaced by |_expandedToolbarConstraints| when animating the
// toolbar expansion.
@property(nonatomic, strong) NSMutableArray* regularToolbarConstraints;
// Constraints used to layout the Toolbar to its expanded state. If these are
// active the locationBarContainer will expand to the size of this VC's view.
// The locationBarView will only expand up to the VC's view safeAreaLayoutGuide.
@property(nonatomic, strong) NSArray* expandedToolbarConstraints;
// Top anchor at the bottom of the safeAreaLayoutGuide. Used so views don't
// overlap with the Status Bar.
@property(nonatomic, strong) NSLayoutYAxisAnchor* topSafeAnchor;

// These constraints pin the content view to the safe area. They are temporarily
// disabled when a fake safe area is simulated by calling
// activateFakeSafeAreaInsets.
@property(nonatomic, strong) NSLayoutConstraint* leadingSafeAreaConstraint;
@property(nonatomic, strong) NSLayoutConstraint* trailingSafeAreaConstraint;
// Leading and trailing safe area constraint for faking a safe area. These
// constraints are activated by calling activateFakeSafeAreaInsets and
// deactivateFakeSafeAreaInsets.
@property(nonatomic, strong) NSLayoutConstraint* leadingFakeSafeAreaConstraint;
@property(nonatomic, strong) NSLayoutConstraint* trailingFakeSafeAreaConstraint;

@property(nonatomic, strong) ToolbarView* view;
@end

@implementation ToolbarViewController
@dynamic view;
@synthesize leadingStackViewButtons = _leadingStackViewButtons;
@synthesize trailingStackViewButtons = _trailingStackViewButtons;
@synthesize backgroundView = _backgroundView;
@synthesize buttonFactory = _buttonFactory;
@synthesize buttonUpdater = _buttonUpdater;
@synthesize dispatcher = _dispatcher;
@synthesize expanded = _expanded;
@synthesize locationBarView = _locationBarView;
@synthesize leadingStackView = _leadingStackView;
@synthesize trailingStackView = _trailingStackView;
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
@synthesize contractButton = _contractButton;
@synthesize locationBarLeadingButton = _locationBarLeadingButton;
@synthesize voiceSearchEnabled = _voiceSearchEnabled;
@synthesize progressBar = _progressBar;
@synthesize locationBarContainerStackView = _locationBarContainerStackView;
@synthesize shadowView = _shadowView;
@synthesize fullBleedShadowView = _fullBleedShadowView;
@synthesize locationBarShadow = _locationBarShadow;
@synthesize expandedToolbarConstraints = _expandedToolbarConstraints;
@synthesize topSafeAnchor = _topSafeAnchor;
@synthesize regularToolbarConstraints = _regularToolbarConstraints;
@synthesize leadingFakeSafeAreaConstraint = _leadingFakeSafeAreaConstraint;
@synthesize trailingFakeSafeAreaConstraint = _trailingFakeSafeAreaConstraint;
@synthesize leadingSafeAreaConstraint = _leadingSafeAreaConstraint;
@synthesize trailingSafeAreaConstraint = _trailingSafeAreaConstraint;
@synthesize omniboxFocuser = _omniboxFocuser;

#pragma mark - Public

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                     buttonFactory:(ToolbarButtonFactory*)buttonFactory
                     buttonUpdater:(ToolbarButtonUpdater*)buttonUpdater
                    omniboxFocuser:(id<OmniboxFocuser>)omniboxFocuser {
  _dispatcher = dispatcher;
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _buttonFactory = buttonFactory;
    _buttonUpdater = buttonUpdater;
    _omniboxFocuser = omniboxFocuser;
    [self setUpToolbarButtons];
    [self setUpLocationBarContainer];
    [self setUpProgressBar];
    _expanded = NO;
  }
  return self;
}

- (void)addToolbarExpansionAnimations:(UIViewPropertyAnimator*)animator {
  // iPad should never try to animate.
  DCHECK(!IsIPadIdiom());
  [NSLayoutConstraint deactivateConstraints:self.regularToolbarConstraints];
  [NSLayoutConstraint activateConstraints:self.expandedToolbarConstraints];
  // By unhiding the button we will make it layout into the correct position in
  // the StackView.
  self.contractButton.hidden = NO;
  self.contractButton.alpha = 0;

  [UIViewPropertyAnimator
      runningPropertyAnimatorWithDuration:ios::material::kDuration2
                                    delay:0
                                  options:UIViewAnimationOptionCurveEaseIn
                               animations:^{
                                 [self
                                     setHorizontalTranslationOffset:
                                         kToolbarButtonAnimationOffset
                                                           forViews:
                                                  self.leadingStackViewButtons];
                                 [self
                                     setHorizontalTranslationOffset:
                                         -kToolbarButtonAnimationOffset
                                                           forViews:
                                                 self.trailingStackViewButtons];
                                 [self setAllToolbarButtonsOpacity:0];
                               }
                               completion:nil];

  [animator addAnimations:^{
    [self.view layoutIfNeeded];
    self.shadowView.alpha = 0;
    self.fullBleedShadowView.alpha = 1;
    self.locationBarShadow.alpha = 0;
  }];

  // If locationBarLeadingButton exists fade it in.
  if (self.locationBarLeadingButton) {
    [self setHorizontalTranslationOffset:-kToolbarButtonAnimationOffset
                                forViews:@[ self.locationBarLeadingButton ]];
    [animator addAnimations:^{
      self.locationBarLeadingButton.hidden = NO;
      [self setHorizontalTranslationOffset:0
                                  forViews:@[ self.locationBarLeadingButton ]];
      self.locationBarLeadingButton.alpha = 1;
    }
                delayFactor:ios::material::kDuration2];
  }

  // When the locationBarContainer has been expanded the Contract button will
  // fade in.
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [self setHorizontalTranslationOffset:kToolbarButtonAnimationOffset
                                forViews:@[ self.contractButton ]];

    [UIViewPropertyAnimator
        runningPropertyAnimatorWithDuration:ios::material::kDuration1
                                      delay:ios::material::kDuration4
                                    options:UIViewAnimationOptionCurveEaseOut
                                 animations:^{
                                   self.contractButton.alpha = 1;
                                   [self
                                       setHorizontalTranslationOffset:0
                                                             forViews:@[
                                                            self.contractButton
                                                             ]];
                                 }
                                 completion:nil];
  }];
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    CGFloat borderWidth = (finalPosition == UIViewAnimatingPositionEnd)
                              ? 0
                              : kLocationBarBorderWidth;
    self.locationBarContainer.layer.borderWidth = borderWidth;
  }];

  self.expanded = YES;
}

- (void)addToolbarContractionAnimations:(UIViewPropertyAnimator*)animator {
  // iPad should never try to animate.
  DCHECK(!IsIPadIdiom());

  // If locationBarLeadingButton exists fade it out before the rest of the
  // Toolbar is contracted.
  if (self.locationBarLeadingButton) {
    [UIViewPropertyAnimator
        runningPropertyAnimatorWithDuration:ios::material::kDuration2
        delay:0
        options:UIViewAnimationOptionCurveEaseIn
        animations:^{
          self.locationBarLeadingButton.alpha = 0;
          [self setHorizontalTranslationOffset:-kToolbarButtonAnimationOffset
                                      forViews:@[
                                        self.locationBarLeadingButton
                                      ]];
        }
        completion:^(UIViewAnimatingPosition finalPosition) {
          [self setHorizontalTranslationOffset:0
                                      forViews:@[
                                        self.locationBarLeadingButton
                                      ]];
        }];
  }

  [NSLayoutConstraint deactivateConstraints:self.expandedToolbarConstraints];
  [NSLayoutConstraint activateConstraints:self.regularToolbarConstraints];
  // Change the Toolbar buttons opacity to 0 since these will fade in once the
  // locationBarContainer has been contracted.
  [self setAllToolbarButtonsOpacity:0];
  [animator addAnimations:^{
    self.locationBarContainer.layer.borderWidth = kLocationBarBorderWidth;
    [self.view layoutIfNeeded];
    self.contractButton.hidden = YES;
    self.contractButton.alpha = 0;
    self.shadowView.alpha = 1;
    self.fullBleedShadowView.alpha = 0;
    self.locationBarShadow.alpha = 1;
    self.locationBarLeadingButton.hidden = YES;
  }];

  // Once the locationBarContainer has been contracted fade in ToolbarButtons.
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [self setHorizontalTranslationOffset:kToolbarButtonAnimationOffset
                                forViews:self.leadingStackViewButtons];
    [self setHorizontalTranslationOffset:-kToolbarButtonAnimationOffset
                                forViews:self.trailingStackViewButtons];
    [UIViewPropertyAnimator
        runningPropertyAnimatorWithDuration:ios::material::kDuration1
                                      delay:ios::material::kDuration4
                                    options:UIViewAnimationOptionCurveEaseOut
                                 animations:^{
                                   [self.view layoutIfNeeded];
                                   [self
                                       setHorizontalTranslationOffset:0
                                                             forViews:
                                                  self.leadingStackViewButtons];
                                   [self
                                       setHorizontalTranslationOffset:0
                                                             forViews:
                                                 self.trailingStackViewButtons];
                                   [self setAllToolbarButtonsOpacity:1];
                                 }
                                 completion:nil];
  }];
  self.expanded = NO;
}

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
  self.progressBar.hidden = YES;
  if (onNTP) {
    self.backgroundView.alpha = 1;
    self.locationBarContainer.hidden = YES;
    // The back button is visible only if the forward button is enabled.
    self.backButton.hiddenInCurrentState = !self.forwardButton.enabled;
  }
}

- (void)resetAfterSideSwipeSnapshot {
  self.backgroundView.alpha = 0;
  self.locationBarContainer.hidden = NO;
  self.backButton.hiddenInCurrentState = NO;
}

- (void)setBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha {
  self.backgroundView.alpha = alpha;
  self.shadowView.alpha = 1 - alpha;
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

- (CGRect)visibleOmniboxFrame {
  CGRect frame = self.locationBarContainer.frame;
  frame =
      [self.view.superview convertRect:frame
                              fromView:[self.locationBarContainer superview]];
  // Needed by the find in page view.
  return CGRectInset(frame, -kBackgroundImageVisibleRectOffset, 0);
}

- (void)locationBarIsFirstResonderOnIPad:(BOOL)isFirstResponder {
  // This is an iPad only function.
  DCHECK(IsIPadIdiom());
  self.bookmarkButton.hiddenInCurrentState = isFirstResponder;
}

- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets {
  self.leadingFakeSafeAreaConstraint.constant =
      UIEdgeInsetsGetLeading(fakeSafeAreaInsets) + [self leadingMargin];
  self.trailingFakeSafeAreaConstraint.constant =
      -UIEdgeInsetsGetTrailing(fakeSafeAreaInsets);
  self.leadingSafeAreaConstraint.active = NO;
  self.trailingSafeAreaConstraint.active = NO;
  self.leadingFakeSafeAreaConstraint.active = YES;
  self.trailingFakeSafeAreaConstraint.active = YES;
}

- (void)deactivateFakeSafeAreaInsets {
  self.leadingFakeSafeAreaConstraint.active = NO;
  self.trailingFakeSafeAreaConstraint.active = NO;
  self.leadingSafeAreaConstraint.active = YES;
  self.trailingSafeAreaConstraint.active = YES;
}

#pragma mark - View lifecyle

- (void)loadView {
  self.view = [[ToolbarView alloc] init];
  self.view.delegate = self;
}

- (void)viewDidLoad {
  // The view can be obstructed by the background view.
  self.view.backgroundColor =
      [self.buttonFactory.toolbarConfiguration backgroundColor];

  [self setUpToolbarStackView];
  [self setUpLocationBarContainerView];
  [self.view addSubview:self.leadingStackView];
  [self.view addSubview:self.trailingStackView];
  // Since the |_locationBarContainer| will expand and cover the stackViews, its
  // important to add it after them so the |_locationBarContainer| has a higher
  // Z order.
  [self.view addSubview:self.locationBarContainer];
  [self.view addSubview:self.shadowView];
  [self.view addSubview:self.fullBleedShadowView];
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
  self.leadingStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.backButton, self.forwardButton, self.reloadButton, self.stopButton
  ]];
  self.leadingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.leadingStackView.distribution = UIStackViewDistributionFill;

  self.trailingStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.shareButton, self.tabSwitchStripButton, self.toolsMenuButton
  ]];
  self.trailingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.trailingStackView.spacing = kStackViewSpacing;
  self.trailingStackView.distribution = UIStackViewDistributionFill;
}

// Sets up the LocationContainerView. Which contains a StackView containing the
// locationBarView, and other buttons like Voice Search, Bookmarks and Contract
// Toolbar.
- (void)setUpLocationBarContainerView {
  self.locationBarContainerStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ self.locationBarView ]];
  self.locationBarContainerStackView.translatesAutoresizingMaskIntoConstraints =
      NO;
  self.locationBarContainerStackView.spacing = kStackViewSpacing;
  self.locationBarContainerStackView.distribution = UIStackViewDistributionFill;
  // Bookmarks and Voice Search buttons will only be part of the Toolbar on
  // iPad. On the other hand the contract button is only needed on non iPad
  // devices, since iPad doesn't animate, thus it doesn't need to contract.
  if (IsIPadIdiom()) {
    [self.locationBarContainerStackView addArrangedSubview:self.bookmarkButton];
    [self.locationBarContainerStackView
        addArrangedSubview:self.voiceSearchButton];
  } else {
    [self.locationBarContainerStackView addArrangedSubview:self.contractButton];
  }
  // If |self.locationBarLeadingButton| exists add it to the StackView.
  if (self.locationBarLeadingButton) {
    [self.locationBarContainerStackView
        insertArrangedSubview:self.locationBarLeadingButton
                      atIndex:0];
  }
  [self.locationBarContainer addSubview:self.locationBarContainerStackView];
}

- (void)setConstraints {
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view.bottomAnchor constraintEqualToAnchor:self.topSafeAnchor
                                         constant:kToolbarHeight]
      .active = YES;

  // ProgressBar constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.progressBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.progressBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.progressBar.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.progressBar.heightAnchor
        constraintEqualToConstant:kProgressBarHeight],
  ]];

  // Shadows constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.shadowView.topAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.shadowView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.shadowView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.shadowView.heightAnchor
        constraintEqualToConstant:kToolbarShadowHeight],
    [self.fullBleedShadowView.topAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.fullBleedShadowView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.fullBleedShadowView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.fullBleedShadowView.heightAnchor
        constraintEqualToConstant:kToolbarFullBleedShadowHeight],
  ]];

  // Stack views constraints.
  // Layout: |[leadingStackView]-[locationBarContainer]-[trailingStackView]|.
  // Safe Area constraints.
  UILayoutGuide* viewSafeAreaGuide = SafeAreaLayoutGuideForView(self.view);
  self.leadingSafeAreaConstraint = [self.leadingStackView.leadingAnchor
      constraintEqualToAnchor:viewSafeAreaGuide.leadingAnchor
                     constant:[self leadingMargin]];
  self.trailingSafeAreaConstraint = [self.trailingStackView.trailingAnchor
      constraintEqualToAnchor:viewSafeAreaGuide.trailingAnchor];
  [NSLayoutConstraint activateConstraints:@[
    self.leadingSafeAreaConstraint, self.trailingSafeAreaConstraint
  ]];

  // Fake safe area constraints. Not activated by default.
  self.leadingFakeSafeAreaConstraint = [self.leadingStackView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor];
  self.trailingFakeSafeAreaConstraint = [self.trailingStackView.trailingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor];

  // Stackviews and locationBar Spacing constraints. These will be disabled when
  // expanding the omnibox.
  NSArray<NSLayoutConstraint*>* stackViewSpacingConstraint = [NSLayoutConstraint
      constraintsWithVisualFormat:
          @"H:[leadingStack]-(spacing)-[locationBar]-(spacing)-[trailingStack]"
                          options:0
                          metrics:@{
                            @"spacing" : @(kHorizontalMargin)
                          }
                            views:@{
                              @"leadingStack" : self.leadingStackView,
                              @"locationBar" : self.locationBarContainer,
                              @"trailingStack" : self.trailingStackView
                            }];
  [self.regularToolbarConstraints
      addObjectsFromArray:stackViewSpacingConstraint];
  // Vertical constraints.
  [NSLayoutConstraint activateConstraints:stackViewSpacingConstraint];
  ApplyVisualConstraintsWithMetrics(
      @[
        @"V:[leadingStack(height)]-(margin)-|",
        @"V:[trailingStack(height)]-(margin)-|"
      ],
      @{
        @"leadingStack" : self.leadingStackView,
        @"trailingStack" : self.trailingStackView
      },
      @{
        @"height" : @(kToolbarHeight - 2 * kButtonVerticalMargin),
        @"margin" : @(kButtonVerticalMargin),
      });

  // LocationBarContainer constraints.
  NSArray* locationBarRegularConstraints = @[
    [self.view.bottomAnchor
        constraintEqualToAnchor:self.locationBarContainer.bottomAnchor
                       constant:kLocationBarVerticalMargin],
    [self.locationBarContainer.heightAnchor
        constraintEqualToConstant:kToolbarHeight -
                                  2 * kLocationBarVerticalMargin],
  ];
  [self.regularToolbarConstraints
      addObjectsFromArray:locationBarRegularConstraints];
  [NSLayoutConstraint activateConstraints:locationBarRegularConstraints];
  // LocationBarContainer shadow constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.locationBarShadow.heightAnchor
        constraintEqualToConstant:kLocationBarShadowHeight],
    [self.locationBarShadow.leadingAnchor
        constraintEqualToAnchor:self.locationBarContainer.leadingAnchor
                       constant:kLocationBarShadowInset],
    [self.locationBarShadow.trailingAnchor
        constraintEqualToAnchor:self.locationBarContainer.trailingAnchor
                       constant:-kLocationBarShadowInset],
    [self.locationBarShadow.topAnchor
        constraintEqualToAnchor:self.locationBarContainer.bottomAnchor
                       constant:-kLocationBarBorderWidth],
  ]];

  // LocationBarStackView constraints. The StackView inside the
  // LocationBarContainer View.
  UILayoutGuide* locationBarContainerSafeAreaGuide =
      SafeAreaLayoutGuideForView(self.locationBarContainer);
  [NSLayoutConstraint activateConstraints:@[
    [self.locationBarContainerStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-(klocationBarStackViewBottomMargin +
                                  kLocationBarVerticalMargin)],
    [self.locationBarContainerStackView.trailingAnchor
        constraintEqualToAnchor:locationBarContainerSafeAreaGuide
                                    .trailingAnchor],
    [self.locationBarContainerStackView.leadingAnchor
        constraintEqualToAnchor:locationBarContainerSafeAreaGuide
                                    .leadingAnchor],
  ]];
}

#pragma mark - Components Setup

- (void)setUpToolbarButtons {
  // Back button.
  self.backButton = [self.buttonFactory backButton];
  self.backButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                   ToolbarComponentVisibilityRegularWidth;
  UILongPressGestureRecognizer* backHistoryLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [self.backButton addGestureRecognizer:backHistoryLongPress];
  [self addStandardActionsForButton:self.backButton];

  // Forward button.
  self.forwardButton = [self.buttonFactory forwardButton];
  self.forwardButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidthOnlyWhenEnabled |
      ToolbarComponentVisibilityRegularWidth;
  UILongPressGestureRecognizer* forwardHistoryLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [self.forwardButton addGestureRecognizer:forwardHistoryLongPress];
  [self addStandardActionsForButton:self.forwardButton];

  // TabSwitcher button.
  self.tabSwitchStripButton = [self.buttonFactory tabSwitcherStripButton];
  self.tabSwitchStripButton.visibilityMask =
      ToolbarComponentVisibilityIPhoneOnly;
  [self addStandardActionsForButton:self.tabSwitchStripButton];

  // Tools menu button.
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.toolsMenuButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                        ToolbarComponentVisibilityRegularWidth;
  [self addStandardActionsForButton:self.toolsMenuButton];

  // Share button.
  self.shareButton = [self.buttonFactory shareButton];
  self.shareButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self addStandardActionsForButton:self.shareButton];

  // Reload button.
  self.reloadButton = [self.buttonFactory reloadButton];
  self.reloadButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self addStandardActionsForButton:self.reloadButton];

  // Stop button.
  self.stopButton = [self.buttonFactory stopButton];
  self.stopButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self addStandardActionsForButton:self.stopButton];

  // Voice Search button.
  self.voiceSearchButton = [self.buttonFactory voiceSearchButton];
  self.voiceSearchButton.visibilityMask =
      ToolbarComponentVisibilityRegularWidth;
  self.voiceSearchButton.enabled = NO;
  [self addStandardActionsForButton:self.voiceSearchButton];

  // Bookmark button.
  self.bookmarkButton = [self.buttonFactory bookmarkButton];
  self.bookmarkButton.visibilityMask = ToolbarComponentVisibilityRegularWidth;
  [self addStandardActionsForButton:self.bookmarkButton];

  // Contract button.
  self.contractButton = [self.buttonFactory contractButton];
  self.contractButton.visibilityMask = ToolbarComponentVisibilityCompactWidth |
                                       ToolbarComponentVisibilityRegularWidth;
  self.contractButton.alpha = 0;
  self.contractButton.hidden = YES;

  // LocationBar LeadingButton
  self.locationBarLeadingButton = [self.buttonFactory locationBarLeadingButton];
  self.locationBarLeadingButton.visibilityMask =
      ToolbarComponentVisibilityCompactWidth |
      ToolbarComponentVisibilityRegularWidth;
  self.locationBarLeadingButton.alpha = 0;
  self.locationBarLeadingButton.hidden = YES;

  // Add buttons to button updater.
  self.buttonUpdater.backButton = self.backButton;
  self.buttonUpdater.forwardButton = self.forwardButton;
  self.buttonUpdater.voiceSearchButton = self.voiceSearchButton;
}

- (void)setUpLocationBarContainer {
  UIView* locationBarContainer = [[UIView alloc] initWithFrame:CGRectZero];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration omniboxBackgroundColor];
  locationBarContainer.layer.borderWidth = kLocationBarBorderWidth;
  locationBarContainer.layer.cornerRadius = kLocationBarShadowHeight;
  locationBarContainer.layer.borderColor =
      [self.buttonFactory.toolbarConfiguration omniboxBorderColor].CGColor;

  self.locationBarShadow =
      [[UIImageView alloc] initWithImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW)];
  self.locationBarShadow.translatesAutoresizingMaskIntoConstraints = NO;
  self.locationBarShadow.userInteractionEnabled = NO;

  [locationBarContainer addSubview:self.locationBarShadow];

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

- (void)didMoveToParentViewController:(UIViewController*)parent {
  UILayoutGuide* omniboxPopupGuide = FindNamedGuide(kOmniboxGuide, self.view);
  AddSameConstraints(self.locationBarContainer, omniboxPopupGuide);
  UILayoutGuide* backButtonGuide = FindNamedGuide(kBackButtonGuide, self.view);
  AddSameConstraints(self.backButton.imageView, backButtonGuide);
  UILayoutGuide* forwardButtonGuide =
      FindNamedGuide(kForwardButtonGuide, self.view);
  AddSameConstraints(self.forwardButton.imageView, forwardButtonGuide);
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
  } else if (self.progressBar.hidden) {
    [self.progressBar setProgress:0];
    [self.progressBar setHidden:NO animated:YES completion:nil];
    // Layout if needed the progress bar to avoir having the progress bar going
    // backward when opening a page from the NTP.
    [self.progressBar layoutIfNeeded];
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

#pragma mark - ToolbarViewFullscreenDelegate

- (void)toolbarViewFrameChanged {
  CGRect frame = self.view.frame;
  CGFloat distanceOffscreen =
      IsIPadIdiom()
          ? fmax((kIPadToolbarY - frame.origin.y) - kScrollFadeDistance, 0)
          : -1 * frame.origin.y;
  CGFloat fraction = 1 - fmin(distanceOffscreen / kScrollFadeDistance, 1);

  self.leadingStackView.alpha = fraction;
  self.locationBarContainer.alpha = fraction;
  self.trailingStackView.alpha = fraction;
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return self.shareButton;
}

#pragma mark - BubbleViewAnchorPointProvider

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  CGPoint anchorPoint = bubble_util::AnchorPoint(
      self.tabSwitchStripButton.imageView.frame, direction);
  return [self.tabSwitchStripButton.imageView.superview
      convertPoint:anchorPoint
            toView:self.tabSwitchStripButton.imageView.window];
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(self.toolsMenuButton.frame, direction);
  return
      [self.toolsMenuButton.superview convertPoint:anchorPoint
                                            toView:self.toolsMenuButton.window];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  self.leadingStackView.alpha = progress;
  self.locationBarContainer.alpha = progress;
  self.trailingStackView.alpha = progress;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)finishFullscreenScrollWithAnimator:
    (FullscreenScrollEndAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^() {
    [self updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - Helper Methods

// Updates all Buttons visibility to match any recent WebState change.
- (void)updateAllButtonsVisibility {
  NSMutableArray* allButtons = [[NSMutableArray alloc] init];
  [allButtons addObjectsFromArray:self.leadingStackViewButtons];
  [allButtons addObjectsFromArray:self.trailingStackViewButtons];
  [allButtons
      addObjectsFromArray:@[ self.voiceSearchButton, self.bookmarkButton ]];
  for (ToolbarButton* button in allButtons) {
    [button updateHiddenInCurrentSizeClass];
  }
}

#pragma mark - Setters & Getters.

- (NSArray<ToolbarButton*>*)leadingStackViewButtons {
  if (!_leadingStackViewButtons) {
    _leadingStackViewButtons =
        [self toolbarButtonsInStackView:self.leadingStackView];
  }
  return _leadingStackViewButtons;
}

- (NSArray<ToolbarButton*>*)trailingStackViewButtons {
  if (!_trailingStackViewButtons) {
    _trailingStackViewButtons =
        [self toolbarButtonsInStackView:self.trailingStackView];
  }
  return _trailingStackViewButtons;
}

- (UIView*)backgroundView {
  if (!_backgroundView) {
    _backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _backgroundView.backgroundColor =
        self.buttonFactory.toolbarConfiguration.NTPBackgroundColor;
    [self.view insertSubview:_backgroundView atIndex:0];
    AddSameConstraints(self.view, _backgroundView);
  }
  return _backgroundView;
}

- (NSLayoutYAxisAnchor*)topSafeAnchor {
  if (!_topSafeAnchor) {
    if (@available(iOS 11, *)) {
      _topSafeAnchor = self.view.safeAreaLayoutGuide.topAnchor;
    } else {
      _topSafeAnchor = self.topLayoutGuide.bottomAnchor;
    }
  }
  return _topSafeAnchor;
}

- (NSMutableArray*)regularToolbarConstraints {
  if (!_regularToolbarConstraints) {
    _regularToolbarConstraints = [[NSMutableArray alloc] init];
  }
  return _regularToolbarConstraints;
}

- (NSArray*)expandedToolbarConstraints {
  if (!_expandedToolbarConstraints) {
    _expandedToolbarConstraints = @[
      [self.locationBarContainer.topAnchor
          constraintEqualToAnchor:self.view.topAnchor],
      [self.locationBarContainer.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
    ];
  }
  return _expandedToolbarConstraints;
}

- (UIView*)locationBarView {
  if (!_locationBarView) {
    _locationBarView = [[UIView alloc] initWithFrame:CGRectZero];
    _locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationBarView
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
    _locationBarView.clipsToBounds = YES;
  }
  return _locationBarView;
}

- (void)setLocationBarView:(UIView*)view {
  if (_locationBarView == view) {
    return;
  }
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [view setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
  _locationBarView = view;
}

- (UIImageView*)shadowView {
  if (!_shadowView) {
    _shadowView = [[UIImageView alloc] init];
    _shadowView.translatesAutoresizingMaskIntoConstraints = NO;
    _shadowView.userInteractionEnabled = NO;
    _shadowView.image = NativeImage(IDR_IOS_TOOLBAR_SHADOW);
  }
  return _shadowView;
}

- (UIImageView*)fullBleedShadowView {
  if (!_fullBleedShadowView) {
    _fullBleedShadowView = [[UIImageView alloc] init];
    _fullBleedShadowView.translatesAutoresizingMaskIntoConstraints = NO;
    _fullBleedShadowView.userInteractionEnabled = NO;
    _fullBleedShadowView.alpha = 0;
    _fullBleedShadowView.image = NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED);
  }
  return _fullBleedShadowView;
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

// Sets all Toolbar Buttons opacity to |alpha|.
- (void)setAllToolbarButtonsOpacity:(CGFloat)alpha {
  for (UIButton* button in [self.leadingStackViewButtons
           arrayByAddingObjectsFromArray:self.trailingStackViewButtons]) {
      button.alpha = alpha;
  }
}

// Offsets the horizontal translation transform of all visible UIViews
// in |array| by |offset|. If the View is hidden it will assign the
// IdentityTransform. Used for fade in animations.
- (void)setHorizontalTranslationOffset:(LayoutOffset)offset
                              forViews:(NSArray<UIView*>*)array {
  for (UIView* view in array) {
    if (!view.hidden)
      view.transform = (offset != 0 && !view.hidden)
                           ? CGAffineTransformMakeTranslation(offset, 0)
                           : CGAffineTransformIdentity;
  }
}

// Returns all the ToolbarButtons in a given UIStackView.
- (NSArray<ToolbarButton*>*)toolbarButtonsInStackView:(UIStackView*)stackView {
  NSMutableArray<ToolbarButton*>* buttons = [[NSMutableArray alloc] init];
  for (UIView* view in stackView.arrangedSubviews) {
    if ([view isKindOfClass:[ToolbarButton class]]) {
      ToolbarButton* button = base::mac::ObjCCastStrict<ToolbarButton>(view);
      [buttons addObject:button];
    }
  }
  return buttons;
}

// Returns the leading margin for the leading stack view.
- (CGFloat)leadingMargin {
  return IsIPadIdiom() ? kLeadingMarginIPad : 0;
}

// Registers the actions which will be triggered when tapping the button.
- (void)addStandardActionsForButton:(UIButton*)button {
  if (button != self.toolsMenuButton) {
    [button addTarget:self.omniboxFocuser
                  action:@selector(cancelOmniboxEdit)
        forControlEvents:UIControlEventTouchUpInside];
  }
  [button addTarget:self
                action:@selector(recordUserMetrics:)
      forControlEvents:UIControlEventTouchUpInside];
}

// Records the use of a button.
- (IBAction)recordUserMetrics:(id)sender {
  if (sender == self.backButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarBack"));
  } else if (sender == self.forwardButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarForward"));
  } else if (sender == self.reloadButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarReload"));
  } else if (sender == self.stopButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarStop"));
  } else if (sender == self.voiceSearchButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarVoiceSearch"));
  } else if (sender == self.bookmarkButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarToggleBookmark"));
  } else if (sender == self.toolsMenuButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenu"));
  } else if (sender == self.tabSwitchStripButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
  } else if (sender == self.shareButton) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShareMenu"));
  } else {
    NOTREACHED();
  }
}
@end
