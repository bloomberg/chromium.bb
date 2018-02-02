// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view_controller.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_foreground_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_to_top_animator.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarViewController ()
// Redefined to be a PrimaryToolbarView.
@property(nonatomic, strong) PrimaryToolbarView* view;
@end

@implementation PrimaryToolbarViewController

@dynamic view;

#pragma mark - Public

- (void)showPrerenderingAnimation {
  __weak PrimaryToolbarViewController* weakSelf = self;
  [self.view.progressBar setProgress:0];
  [self.view.progressBar setHidden:NO
                          animated:YES
                        completion:^(BOOL finished) {
                          [weakSelf stopProgressBar];
                        }];
}

#pragma mark - AdaptiveToolbarViewController

- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP {
  [super updateForSideSwipeSnapshotOnNTP:onNTP];
  if (!onNTP)
    return;

  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.NTPBackgroundColor;
  self.view.locationBarContainer.hidden = YES;
}

- (void)resetAfterSideSwipeSnapshot {
  [super resetAfterSideSwipeSnapshot];
  self.view.backgroundColor = nil;
  self.view.locationBarContainer.hidden = NO;
}

#pragma mark - UIViewController

- (void)loadView {
  DCHECK(self.buttonFactory);

  self.view =
      [[PrimaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];

  // This method cannot be called from the init as the topSafeAnchor can only be
  // set to topLayoutGuide after the view creation on iOS 10.
  [self.view setUp];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Adds the layout guide to the buttons.
  self.view.toolsMenuButton.guideName = kTabSwitcherGuide;
  self.view.forwardLeadingButton.guideName = kForwardButtonGuide;
  self.view.forwardTrailingButton.guideName = kForwardButtonGuide;
  self.view.backButton.guideName = kBackButtonGuide;

  // Add navigation popup menu triggers.
  [self addLongPressGestureToView:self.view.backButton];
  [self addLongPressGestureToView:self.view.forwardLeadingButton];
  [self addLongPressGestureToView:self.view.forwardTrailingButton];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  ConstrainNamedGuideToView(kOmniboxGuide, self.view.locationBarContainer);
}

#pragma mark - Property accessors

- (void)setLocationBarView:(UIView*)locationBarView {
  self.view.locationBarView = locationBarView;
}

#pragma mark - ActivityServicePositioner

- (UIView*)shareButtonView {
  return self.view.shareButton;
}

#pragma mark - TabHistoryUIUpdater

- (void)updateUIForTabHistoryPresentationFrom:(ToolbarButtonType)buttonType {
  if (buttonType == ToolbarButtonTypeBack) {
    self.view.backButton.selected = YES;
  } else {
    self.view.forwardLeadingButton.selected = YES;
    self.view.forwardTrailingButton.selected = YES;
  }
}

- (void)updateUIForTabHistoryWasDismissed {
  self.view.backButton.selected = NO;
  self.view.forwardLeadingButton.selected = NO;
  self.view.forwardTrailingButton.selected = NO;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax(progress * 2 - 1, 0);
  self.view.leadingStackView.alpha = alphaValue;
  self.view.trailingStackView.alpha = alphaValue;
  self.view.locationBarHeight.constant =
      AlignValueToPixel(kToolbarHeightFullscreen +
                        (kToolbarHeight - kToolbarHeightFullscreen) * progress -
                        2 * kLocationBarVerticalMargin);
  self.view.locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration
          locationBarBackgroundColorWithVisibility:alphaValue];
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)finishFullscreenScrollWithAnimator:
    (FullscreenScrollEndAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

- (void)scrollFullscreenToTopWithAnimator:
    (FullscreenScrollToTopAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

- (void)showToolbarForForgroundWithAnimator:
    (FullscreenForegroundAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

#pragma mark - FullscreenUIElement helpers

- (void)addFullscreenAnimationsToAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [self updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - ToolbarAnimatee

- (void)expandLocationBar {
  [NSLayoutConstraint deactivateConstraints:self.view.unfocusedConstraints];
  [NSLayoutConstraint activateConstraints:self.view.focusedConstraints];
  [self.view layoutIfNeeded];
}

- (void)contractLocationBar {
  [NSLayoutConstraint deactivateConstraints:self.view.focusedConstraints];
  [NSLayoutConstraint activateConstraints:self.view.unfocusedConstraints];
  [self.view layoutIfNeeded];
}

- (void)showCancelButton {
  self.view.cancelButton.hidden = NO;
}

- (void)hideCancelButton {
  self.view.cancelButton.hidden = YES;
}

- (void)showControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 1;
  }
}

- (void)hideControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 0;
  }
}

#pragma mark - Private

// Adds a LongPressGesture to the |view|, with target on -|handleLongPress:|.
- (void)addLongPressGestureToView:(UIView*)view {
  UILongPressGestureRecognizer* navigationHistoryLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [view addGestureRecognizer:navigationHistoryLongPress];
}

// Handles the long press on the views.
- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  if (gesture.view == self.view.backButton) {
    [self.dispatcher showTabHistoryPopupForBackwardHistory];
  } else if (gesture.view == self.view.forwardLeadingButton ||
             gesture.view == self.view.forwardTrailingButton) {
    [self.dispatcher showTabHistoryPopupForForwardHistory];
  }
}

@end
