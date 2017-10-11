// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LegacyToolbarCoordinator
@synthesize tabModel = _tabModel;
@synthesize webToolbarController = _webToolbarController;

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
