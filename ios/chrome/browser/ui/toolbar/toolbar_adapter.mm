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

- (ToolbarButtonUpdater*)buttonUpdater {
  return self.toolbarCoordinator.buttonUpdater;
}

- (UIViewController*)viewController {
  return self.toolbarCoordinator.viewController;
}

#pragma mark - Abstract WebToolbar

- (void)browserStateDestroyed {
  [self.toolbarCoordinator stop];
}

- (void)updateToolbarState {
  [self.toolbarCoordinator updateToolbarState];
}

- (void)showPrerenderingAnimation {
  [self.toolbarCoordinator showPrerenderingAnimation];
}

- (void)currentPageLoadStarted {
  // No op, the mediator is taking care of this.
}

- (CGRect)visibleOmniboxFrame {
  // No-op. The Clean Toolbar uses named layout guides.
  return CGRectZero;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.toolbarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.toolbarCoordinator showingOmniboxPopup];
}

- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab {
  [self.toolbarCoordinator updateToolbarForSideSwipeSnapshot:tab.webState];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.toolbarCoordinator resetToolbarAfterSideSwipeSnapshot];
}

#pragma mark - Abstract Toolbar

- (void)setShareButtonEnabled:(BOOL)enabled {
  // No op.
}

- (void)triggerToolsMenuButtonAnimation {
  [self.toolbarCoordinator triggerToolsMenuButtonAnimation];
}

- (void)adjustToolbarHeight {
  return;
}

- (void)setBackgroundAlpha:(CGFloat)alpha {
  [self.toolbarCoordinator setBackgroundToIncognitoNTPColorWithAlpha:1 - alpha];
}

- (void)setTabCount:(NSInteger)tabCount {
  // No op.
}

- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets {
  [self.toolbarCoordinator activateFakeSafeAreaInsets:fakeSafeAreaInsets];
}

- (void)deactivateFakeSafeAreaInsets {
  [self.toolbarCoordinator deactivateFakeSafeAreaInsets];
}

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible {
  [self.toolbarCoordinator setToolsMenuIsVisibleForToolsMenuButton:isVisible];
}

- (void)start {
  [self.toolbarCoordinator start];
}

- (void)stop {
  [self.toolbarCoordinator stop];
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

- (UIView*)shareButtonView {
  return [[self.toolbarCoordinator activityServicePositioner] shareButtonView];
}

#pragma mark - QRScannerResultLoading

- (void)receiveQRScannerResult:(NSString*)qrScannerResult
               loadImmediately:(BOOL)load {
  [self.toolbarCoordinator receiveQRScannerResult:qrScannerResult
                                  loadImmediately:load];
}

#pragma mark - BubbleViewAnchorPointProvider

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  // No-op. The Clean Toolbar uses named layout guides.
  return CGPointZero;
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  // No-op. The Clean Toolbar uses named layout guides.
  return CGPointZero;
}

#pragma mark - FullscreenUIElement

// FullscreenUIElements in this adapter are no-ops.  ToolbarCoordinator updates
// its view controller's UI for fullscreen events.

- (void)updateForFullscreenProgress:(CGFloat)progress {
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
}

- (void)finishFullscreenScrollWithAnimator:
    (FullscreenScrollEndAnimator*)animator {
}

#pragma mark - ToolsMenuPresentationProvider

- (UIButton*)presentingButtonForToolsMenuCoordinator:
    (ToolsMenuCoordinator*)coordinator {
  return [self.toolbarCoordinator
      presentingButtonForToolsMenuCoordinator:coordinator];
}

@end
