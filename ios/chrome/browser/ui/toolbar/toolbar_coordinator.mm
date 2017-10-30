// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LegacyToolbarCoordinator
@synthesize tabModel = _tabModel;
@synthesize webToolbarController = _webToolbarController;

- (ToolbarController*)toolbarController {
  return self.webToolbarController;
}

- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate {
  return self.webToolbarController;
}

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.webToolbarController;
}

- (id<TabHistoryPositioner>)tabHistoryPositioner {
  return self.webToolbarController;
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  return self.webToolbarController;
}

- (id<QRScannerResultLoading>)QRScannerResultLoader {
  return self.webToolbarController;
}

- (void)setWebToolbar:(WebToolbarController*)webToolbarController {
  self.webToolbarController = webToolbarController;
}

- (void)setToolbarDelegate:(id<WebToolbarDelegate>)delegate {
  self.webToolbarController.delegate = delegate;
}

#pragma mark - Public

- (void)adjustToolbarHeight {
  self.webToolbarController.heightConstraint.constant =
      ToolbarHeightWithTopOfScreenOffset(
          [self.webToolbarController statusBarOffset]);
  self.webToolbarController.heightConstraint.active = YES;
}

#pragma mark - WebToolbarController interface

- (void)selectedTabChanged {
  [self.webToolbarController selectedTabChanged];
}

- (void)setTabCount:(NSInteger)tabCount {
  [self.webToolbarController setTabCount:tabCount];
}

- (void)browserStateDestroyed {
  [self.webToolbarController setBackgroundAlpha:1.0];
  [self.webToolbarController browserStateDestroyed];
}

- (void)updateToolbarState {
  [self.webToolbarController updateToolbarState];
}

- (void)setShareButtonEnabled:(BOOL)enabled {
  [self.webToolbarController setShareButtonEnabled:enabled];
}

- (void)showPrerenderingAnimation {
  [self.webToolbarController showPrerenderingAnimation];
}

- (BOOL)isOmniboxFirstResponder {
  return [self.webToolbarController isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.webToolbarController showingOmniboxPopup];
}

- (void)currentPageLoadStarted {
  [self.webToolbarController currentPageLoadStarted];
}

- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration {
  [self.webToolbarController showToolsMenuPopupWithConfiguration:configuration];
}

- (ToolsPopupController*)toolsPopupController {
  return [self.webToolbarController toolsPopupController];
}

- (void)dismissToolsMenuPopup {
  [self.webToolbarController dismissToolsMenuPopup];
}

- (CGRect)visibleOmniboxFrame {
  return [self.webToolbarController visibleOmniboxFrame];
}

- (void)triggerToolsMenuButtonAnimation {
  [self.webToolbarController triggerToolsMenuButtonAnimation];
}

- (UIView*)view {
  return [self.webToolbarController view];
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  [self.webToolbarController focusOmnibox];
}

- (void)cancelOmniboxEdit {
  [self.webToolbarController cancelOmniboxEdit];
}

- (void)focusFakebox {
  [self.webToolbarController focusFakebox];
}

- (void)onFakeboxBlur {
  [self.webToolbarController onFakeboxBlur];
}

- (void)onFakeboxAnimationComplete {
  [self.webToolbarController onFakeboxAnimationComplete];
}

#pragma mark - SideSwipeToolbarInteracting

- (UIView*)toolbarView {
  return self.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab {
  [self.webToolbarController updateToolbarForSideSwipeSnapshot:tab];
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.webToolbarController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self.webToolbarController resetToolbarAfterSideSwipeSnapshot];
  return toolbarSnapshot;
}

#pragma mark - ToolbarSnapshotProviding

- (UIView*)snapshotForTabSwitcher {
  UIView* toolbarSnapshotView;
  if ([self.view window]) {
    toolbarSnapshotView = [self.view snapshotViewAfterScreenUpdates:NO];
  } else {
    toolbarSnapshotView = [[UIView alloc] initWithFrame:self.view.frame];
    [toolbarSnapshotView layer].contents = static_cast<id>(
        CaptureViewWithOption(self.view, 1, kClientSideRendering).CGImage);
  }
  return toolbarSnapshotView;
}

#pragma mark - IncognitoViewControllerDelegate

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  [self.webToolbarController setBackgroundAlpha:alpha];
}

#pragma mark - BubbleViewAnchorPointProvider methods.

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  return [self.webToolbarController anchorPointForTabSwitcherButton:direction];
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  return [self.webToolbarController anchorPointForToolsMenuButton:direction];
}
@end
