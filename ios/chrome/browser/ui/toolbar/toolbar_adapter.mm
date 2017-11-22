// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_adapter.h"

#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_delegate.h"

@interface ToolbarAdapter ()
@property(nonatomic, strong) ToolbarCoordinator* toolbarCoordinator;
@end

@implementation ToolbarAdapter
@synthesize backgroundView = _backgroundView;
@synthesize buttonUpdater = _buttonAdapter;
@synthesize toolbarCoordinator = _toolbarCoordinator;
@synthesize delegate = _delegate;
@synthesize toolsPopupController = _toolsPopupController;
@synthesize URLLoader = _URLLoader;

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

- (UIViewController*)viewController {
  return self.toolbarCoordinator.viewController;
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

- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab {
  [self.toolbarCoordinator updateToolbarForSideSwipeSnapshot:tab.webState];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.toolbarCoordinator resetToolbarAfterSideSwipeSnapshot];
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

- (void)setToolsMenuStateProvider:
    (id<ToolsMenuPresentationStateProvider>)provider {
  return;
}

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible {
  return;
}

- (void)start {
  [self.toolbarCoordinator start];
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  [self.toolbarCoordinator focusOmnibox];
}

- (void)cancelOmniboxEdit {
  [self.toolbarCoordinator cancelOmniboxEdit];
}

- (void)focusFakebox {
  [self.toolbarCoordinator focusFakebox];
}

- (void)onFakeboxBlur {
  [self.toolbarCoordinator onFakeboxBlur];
}

- (void)onFakeboxAnimationComplete {
  [self.toolbarCoordinator onFakeboxAnimationComplete];
}

#pragma mark - VoiceSearchControllerDelegate

- (void)receiveVoiceSearchResult:(NSString*)voiceResult {
  [self.toolbarCoordinator receiveVoiceSearchResult:voiceResult];
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
  [self.toolbarCoordinator receiveQRScannerResult:qrScannerResult
                                  loadImmediately:load];
}

#pragma mark - BubbleViewAnchorPointProvider

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  return CGPointZero;
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  return CGPointZero;
}

@end
