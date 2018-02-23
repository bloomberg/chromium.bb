// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/legacy_toolbar_coordinator.h"

#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_coordinator.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyToolbarCoordinator ()<ToolbarCommands> {
  // Coordinator for the tools menu UI.
  ToolsMenuCoordinator* _toolsMenuCoordinator;
  // The fullscreen updater.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUpdater;
}

@property(nonatomic, strong) ToolbarCoordinator* toolbarCoordinator;
@end

@implementation LegacyToolbarCoordinator
@synthesize toolbarCoordinator = _toolbarCoordinator;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
            toolsMenuConfigurationProvider:
                (id<ToolsMenuConfigurationProvider>)configurationProvider
                                dispatcher:(CommandDispatcher*)dispatcher
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  if (self = [super initWithBaseViewController:viewController
                                  browserState:browserState]) {
    DCHECK(browserState);
    _toolbarCoordinator = [[ToolbarCoordinator alloc] init];
    _toolbarCoordinator.webStateList = webStateList;
    _toolbarCoordinator.dispatcher =
        static_cast<id<ApplicationCommands, BrowserCommands, OmniboxFocuser>>(
            dispatcher);
    _toolbarCoordinator.browserState = browserState;

    _toolsMenuCoordinator = [[ToolsMenuCoordinator alloc] init];
    _toolsMenuCoordinator.dispatcher = dispatcher;
    _toolsMenuCoordinator.configurationProvider = configurationProvider;
    _toolsMenuCoordinator.presentationProvider = _toolbarCoordinator;
    [_toolsMenuCoordinator start];

    [dispatcher startDispatchingToTarget:self
                             forProtocol:@protocol(ToolbarCommands)];

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(toolsMenuWillShowNotification:)
                          name:kToolsMenuWillShowNotification
                        object:_toolsMenuCoordinator];
    [defaultCenter addObserver:self
                      selector:@selector(toolsMenuWillHideNotification:)
                          name:kToolsMenuWillHideNotification
                        object:_toolsMenuCoordinator];
  }
  return self;
}

- (void)start {
  [self.toolbarCoordinator start];
}

- (void)stop {
  [self.toolbarCoordinator setBackgroundToIncognitoNTPColorWithAlpha:0];
  [self.toolbarCoordinator stop];
  self.toolbarCoordinator = nil;
}

- (UIViewController*)viewController {
  return self.toolbarCoordinator.viewController;
}

- (void)setURLLoader:(id<UrlLoader>)URLLoader {
  self.toolbarCoordinator.URLLoader = URLLoader;
}

- (id<UrlLoader>)URLLoader {
  return self.toolbarCoordinator.URLLoader;
}

- (void)setDelegate:(id<ToolbarCoordinatorDelegate>)delegate {
  self.toolbarCoordinator.delegate = delegate;
}

- (id<ToolbarCoordinatorDelegate>)delegate {
  return self.toolbarCoordinator.delegate;
}

#pragma mark - Delegates

- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate {
  return self.toolbarCoordinator.voiceSearchControllerDelegate;
}

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.toolbarCoordinator.activityServicePositioner;
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  return self.toolbarCoordinator.buttonUpdater;
}

- (id<QRScannerResultLoading>)QRScannerResultLoader {
  return self.toolbarCoordinator.QRScannerResultLoader;
}

- (id<OmniboxFocuser>)omniboxFocuser {
  return self.toolbarCoordinator.omniboxFocuser;
}

#pragma mark - ToolbarCommands

- (void)triggerToolsMenuButtonAnimation {
  [self.toolbarCoordinator triggerToolsMenuButtonAnimation];
}

#pragma mark - ToolbarCoordinating

- (void)updateToolsMenu {
  [_toolsMenuCoordinator updateConfiguration];
}

#pragma mark - PrimaryToolbarCoordinator

- (void)showPrerenderingAnimation {
  [self.toolbarCoordinator showPrerenderingAnimation];
}

- (BOOL)isOmniboxFirstResponder {
  return [self.toolbarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.toolbarCoordinator showingOmniboxPopup];
}

- (void)transitionToLocationBarFocusedState:(BOOL)focused {
  [self.toolbarCoordinator transitionToLocationBarFocusedState:focused];
}

#pragma mark - FakeboxFocuser

- (void)focusFakebox {
  [self.toolbarCoordinator focusFakebox];
}

- (void)onFakeboxBlur {
  [self.toolbarCoordinator onFakeboxBlur];
}

- (void)onFakeboxAnimationComplete {
  [self.toolbarCoordinator onFakeboxAnimationComplete];
}

#pragma mark - SideSwipeToolbarInteracting

- (UIView*)toolbarView {
  return self.viewController.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState {
  [self.toolbarCoordinator updateToolbarForSideSwipeSnapshot:webState];
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.viewController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self.toolbarCoordinator resetToolbarAfterSideSwipeSnapshot];
  return toolbarSnapshot;
}

#pragma mark - ToolbarSnapshotProviding

- (UIView*)snapshotForTabSwitcher {
  UIView* toolbarSnapshotView;
  if ([self.viewController.view window]) {
    toolbarSnapshotView =
        [self.viewController.view snapshotViewAfterScreenUpdates:NO];
  } else {
    toolbarSnapshotView =
        [[UIView alloc] initWithFrame:self.viewController.view.frame];
    [toolbarSnapshotView layer].contents = static_cast<id>(
        CaptureViewWithOption(self.viewController.view, 0, kClientSideRendering)
            .CGImage);
  }
  return toolbarSnapshotView;
}

- (UIView*)snapshotForStackViewWithWidth:(CGFloat)width
                          safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGRect oldFrame = self.viewController.view.superview.frame;
  CGRect newFrame = oldFrame;
  newFrame.size.width = width;

  self.viewController.view.superview.frame = newFrame;
  [self.toolbarCoordinator activateFakeSafeAreaInsets:safeAreaInsets];
  [self.viewController.view.superview layoutIfNeeded];

  UIView* toolbarSnapshotView = [self snapshotForTabSwitcher];

  self.viewController.view.superview.frame = oldFrame;
  [self.toolbarCoordinator deactivateFakeSafeAreaInsets];

  return toolbarSnapshotView;
}

- (UIColor*)toolbarBackgroundColor {
  return [self.toolbarCoordinator toolbarBackgroundColor];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  [self.toolbarCoordinator setBackgroundToIncognitoNTPColorWithAlpha:1 - alpha];
}

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  NOTREACHED();
}

#pragma mark - ToolsMenuPresentationStateProvider

- (BOOL)isShowingToolsMenu {
  return [_toolsMenuCoordinator isShowingToolsMenu];
}

#pragma mark - Tools Menu

- (void)toolsMenuWillShowNotification:(NSNotification*)note {
  [self.toolbarCoordinator setToolsMenuIsVisibleForToolsMenuButton:YES];
}

- (void)toolsMenuWillHideNotification:(NSNotification*)note {
  [self.toolbarCoordinator setToolsMenuIsVisibleForToolsMenuButton:NO];
}

@end
