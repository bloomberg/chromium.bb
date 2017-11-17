// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_adapter.h"

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_delegate.h"

@interface ToolbarAdapter ()
@property(nonatomic, strong) ToolbarCoordinator* toolbarCoordinator;
@end

@implementation ToolbarAdapter
@synthesize backgroundView = _backgroundView;
@synthesize toolbarCoordinator = _toolbarCoordinator;
@synthesize delegate = _delegate;
@synthesize toolsPopupController = _toolsPopupController;
@synthesize URLLoader = _URLLoader;
@synthesize viewController = _viewController;

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                      browserState:(ios::ChromeBrowserState*)browserState
                      webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _toolbarCoordinator = [[ToolbarCoordinator alloc] init];
    _toolbarCoordinator.webStateList = webStateList;
    _toolbarCoordinator.dispatcher = dispatcher;
    _toolbarCoordinator.browserState = browserState;
  }
  return self;
}

#pragma mark - Properties

- (void)setDelegate:(id<WebToolbarDelegate>)delegate {
  _delegate = delegate;
  self.toolbarCoordinator.delegate = delegate;
}

- (void)setURLLoader:(id<UrlLoader>)URLLoader {
  _URLLoader = URLLoader;
  self.toolbarCoordinator.URLLoader = URLLoader;
}

#pragma mark - Abstract WebToolbar

- (void)browserStateDestroyed {
  return;
}

- (void)updateToolbarState {
  return;
}

- (void)showPrerenderingAnimation {
  return;
}

- (void)currentPageLoadStarted {
  return;
}

- (CGRect)visibleOmniboxFrame {
  return CGRectZero;
}

- (BOOL)isOmniboxFirstResponder {
  return NO;
}

- (BOOL)showingOmniboxPopup {
  return NO;
}

- (void)selectedTabChanged {
  return;
}

- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab {
  return;
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  return;
}

#pragma mark - Abstract Toolbar

- (void)setShareButtonEnabled:(BOOL)enabled {
  return;
}

- (void)triggerToolsMenuButtonAnimation {
  return;
}

- (void)adjustToolbarHeight {
  return;
}

- (void)setBackgroundAlpha:(CGFloat)alpha {
  return;
}

- (void)setTabCount:(NSInteger)tabCount {
  return;
}

- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets {
  return;
}

- (void)deactivateFakeSafeAreaInsets {
  return;
}

- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration {
  return;
}

- (void)dismissToolsMenuPopup {
  return;
}

#pragma mark - Omnibox Focuser

- (void)focusOmnibox {
  return;
}

- (void)cancelOmniboxEdit {
  return;
}

- (void)focusFakebox {
  return;
}

- (void)onFakeboxBlur {
  return;
}

- (void)onFakeboxAnimationComplete {
  return;
}

#pragma mark - VoiceSearchControllerDelegate

- (void)receiveVoiceSearchResult:(NSString*)voiceResult {
  return;
}

#pragma mark - ActivityServicePositioner

- (CGRect)shareButtonAnchorRect {
  return CGRectZero;
}

- (UIView*)shareButtonView {
  return nil;
}

#pragma mark - TabHistoryPositioner

- (CGPoint)originPointForToolbarButton:(ToolbarButtonType)toolbarButton {
  return CGPointZero;
}

#pragma mark - TabHistoryUIUpdater

- (void)updateUIForTabHistoryPresentationFrom:(ToolbarButtonType)button {
  return;
}

- (void)updateUIForTabHistoryWasDismissed {
  return;
}

#pragma mark - QRScannerResultLoading

- (void)receiveQRScannerResult:(NSString*)qrScannerResult
               loadImmediately:(BOOL)load {
  return;
}

#pragma mark - BubbleViewAnchorPointProvider

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  return CGPointZero;
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  return CGPointZero;
}

@end
