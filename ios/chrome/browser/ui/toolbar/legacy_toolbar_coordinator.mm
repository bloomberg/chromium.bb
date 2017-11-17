// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/legacy_toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyToolbarCoordinator ()
@property(nonatomic, strong) id<Toolbar> toolbarController;
@end

@implementation LegacyToolbarCoordinator
@synthesize tabModel = _tabModel;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize toolbarController = _toolbarController;

- (void)stop {
  self.toolbarController = nil;
}

- (UIViewController*)toolbarViewController {
  if (!_toolbarViewController)
    _toolbarViewController = self.toolbarController.viewController;

  return _toolbarViewController;
}

- (void)setToolbarController:(id<Toolbar>)toolbarController {
  _toolbarController = toolbarController;
}

- (void)setToolbarDelegate:(id<WebToolbarDelegate>)delegate {
  self.toolbarController.delegate = delegate;
}

#pragma mark - Delegates

- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate {
  return self.toolbarController;
}

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.toolbarController;
}

- (id<TabHistoryPositioner>)tabHistoryPositioner {
  return self.toolbarController.buttonUpdater;
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  return self.toolbarController.buttonUpdater;
}

- (id<QRScannerResultLoading>)QRScannerResultLoader {
  return self.toolbarController;
}

#pragma mark - WebToolbarController public interface

- (void)adjustToolbarHeight {
  [self.toolbarController adjustToolbarHeight];
}

- (void)selectedTabChanged {
  [self.toolbarController selectedTabChanged];
}

- (void)setTabCount:(NSInteger)tabCount {
  [self.toolbarController setTabCount:tabCount];
}

- (void)browserStateDestroyed {
  [self.toolbarController setBackgroundAlpha:1.0];
  [self.toolbarController browserStateDestroyed];
}

- (void)updateToolbarState {
  [self.toolbarController updateToolbarState];
}

- (void)setShareButtonEnabled:(BOOL)enabled {
  [self.toolbarController setShareButtonEnabled:enabled];
}

- (void)showPrerenderingAnimation {
  [self.toolbarController showPrerenderingAnimation];
}

- (BOOL)isOmniboxFirstResponder {
  return [self.toolbarController isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.toolbarController showingOmniboxPopup];
}

- (void)currentPageLoadStarted {
  [self.toolbarController currentPageLoadStarted];
}

- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration {
  [self.toolbarController showToolsMenuPopupWithConfiguration:configuration];
}

- (ToolsPopupController*)toolsPopupController {
  return [self.toolbarController toolsPopupController];
}

- (void)dismissToolsMenuPopup {
  [self.toolbarController dismissToolsMenuPopup];
}

- (CGRect)visibleOmniboxFrame {
  return [self.toolbarController visibleOmniboxFrame];
}

- (void)triggerToolsMenuButtonAnimation {
  [self.toolbarController triggerToolsMenuButtonAnimation];
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  [self.toolbarController focusOmnibox];
}

- (void)cancelOmniboxEdit {
  [self.toolbarController cancelOmniboxEdit];
}

- (void)focusFakebox {
  [self.toolbarController focusFakebox];
}

- (void)onFakeboxBlur {
  [self.toolbarController onFakeboxBlur];
}

- (void)onFakeboxAnimationComplete {
  [self.toolbarController onFakeboxAnimationComplete];
}

#pragma mark - SideSwipeToolbarInteracting

- (UIView*)toolbarView {
  return self.toolbarViewController.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab {
  [self.toolbarController updateToolbarForSideSwipeSnapshot:tab];
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.toolbarViewController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self.toolbarController resetToolbarAfterSideSwipeSnapshot];
  return toolbarSnapshot;
}

#pragma mark - ToolbarSnapshotProviding

- (UIView*)snapshotForTabSwitcher {
  UIView* toolbarSnapshotView;
  if ([self.toolbarViewController.view window]) {
    toolbarSnapshotView =
        [self.toolbarViewController.view snapshotViewAfterScreenUpdates:NO];
  } else {
    toolbarSnapshotView =
        [[UIView alloc] initWithFrame:self.toolbarViewController.view.frame];
    [toolbarSnapshotView layer].contents =
        static_cast<id>(CaptureViewWithOption(self.toolbarViewController.view,
                                              0, kClientSideRendering)
                            .CGImage);
  }
  return toolbarSnapshotView;
}

- (UIView*)snapshotForStackViewWithWidth:(CGFloat)width
                          safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGRect oldFrame = self.toolbarViewController.view.frame;
  CGRect newFrame = oldFrame;
  newFrame.size.width = width;

  self.toolbarViewController.view.frame = newFrame;
  [self.toolbarController activateFakeSafeAreaInsets:safeAreaInsets];

  UIView* toolbarSnapshotView = [self snapshotForTabSwitcher];

  self.toolbarViewController.view.frame = oldFrame;
  [self.toolbarController deactivateFakeSafeAreaInsets];

  return toolbarSnapshotView;
}

- (UIColor*)toolbarBackgroundColor {
  UIColor* toolbarBackgroundColor = nil;
  if (self.toolbarController.backgroundView.hidden ||
      self.toolbarController.backgroundView.alpha == 0) {
    // If the background view isn't visible, use the base toolbar view's
    // background color.
    toolbarBackgroundColor = self.toolbarViewController.view.backgroundColor;
  }
  return toolbarBackgroundColor;
}

#pragma mark - IncognitoViewControllerDelegate

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  [self.toolbarController setBackgroundAlpha:alpha];
}

#pragma mark - BubbleViewAnchorPointProvider methods.

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  return [self.toolbarController anchorPointForTabSwitcherButton:direction];
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  return [self.toolbarController anchorPointForToolsMenuButton:direction];
}
@end
