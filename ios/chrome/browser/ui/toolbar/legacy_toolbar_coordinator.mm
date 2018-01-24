// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/legacy_toolbar_coordinator.h"

#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
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

@property(nonatomic, strong) id<Toolbar> toolbarController;
@end

@implementation LegacyToolbarCoordinator
@synthesize toolbarController = _toolbarController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
            toolsMenuConfigurationProvider:
                (id<ToolsMenuConfigurationProvider>)configurationProvider
                                dispatcher:(CommandDispatcher*)dispatcher
                              browserState:
                                  (ios::ChromeBrowserState*)browserState {
  if (self = [super initWithBaseViewController:viewController
                                  browserState:browserState]) {
    DCHECK(browserState);
    _toolsMenuCoordinator = [[ToolsMenuCoordinator alloc] init];
    _toolsMenuCoordinator.dispatcher = dispatcher;
    _toolsMenuCoordinator.configurationProvider = configurationProvider;
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
  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen))
    [self startObservingFullscreen];
}

- (void)stop {
  [self.toolbarController setBackgroundAlpha:1.0];
  [self.toolbarController browserStateDestroyed];
  [self.toolbarController stop];
  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen))
    [self stopObservingFullscreen];
  self.toolbarController = nil;
}

- (UIViewController*)viewController {
  return self.toolbarController.viewController;
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

- (id<OmniboxFocuser>)omniboxFocuser {
  return self.toolbarController;
}

#pragma mark - WebToolbarController public interface

- (void)setToolbarController:(id<Toolbar>)toolbarController {
  _toolbarController = toolbarController;
  // ToolbarController needs to know about whether the tools menu is presented
  // or not, and does so by storing a reference to the coordinator to query.
  _toolsMenuCoordinator.presentationProvider = _toolbarController;
  [toolbarController start];
}

- (void)adjustToolbarHeight {
  [self.toolbarController adjustToolbarHeight];
}

- (void)selectedTabChanged {
  [self.toolbarController cancelOmniboxEdit];
}

- (void)setTabCount:(NSInteger)tabCount {
  [self.toolbarController setTabCount:tabCount];
}

- (void)browserStateDestroyed {
  [self stop];
}

- (void)updateToolbarState {
  [self.toolbarController updateToolbarState];
  [_toolsMenuCoordinator updateConfiguration];
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

- (CGRect)visibleOmniboxFrame {
  return [self.toolbarController visibleOmniboxFrame];
}

- (void)triggerToolsMenuButtonAnimation {
  [self.toolbarController triggerToolsMenuButtonAnimation];
}

#pragma mark - FakeboxFocuser

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
  return self.viewController.view;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self isOmniboxFirstResponder] && ![self showingOmniboxPopup];
}

- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab {
  [self.toolbarController updateToolbarForSideSwipeSnapshot:tab];
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.viewController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self.toolbarController resetToolbarAfterSideSwipeSnapshot];
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
  [self.toolbarController activateFakeSafeAreaInsets:safeAreaInsets];
  [self.viewController.view.superview layoutIfNeeded];

  UIView* toolbarSnapshotView = [self snapshotForTabSwitcher];

  self.viewController.view.superview.frame = oldFrame;
  [self.toolbarController deactivateFakeSafeAreaInsets];

  return toolbarSnapshotView;
}

- (UIColor*)toolbarBackgroundColor {
  UIColor* toolbarBackgroundColor = nil;
  if (self.toolbarController.backgroundView.hidden ||
      self.toolbarController.backgroundView.alpha == 0) {
    // If the background view isn't visible, use the base toolbar view's
    // background color.
    toolbarBackgroundColor = self.viewController.view.backgroundColor;
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

#pragma mark - ToolsMenuPresentationStateProvider

- (BOOL)isShowingToolsMenu {
  return [_toolsMenuCoordinator isShowingToolsMenu];
}

#pragma mark - Tools Menu

- (void)toolsMenuWillShowNotification:(NSNotification*)note {
  [self.toolbarController setToolsMenuIsVisibleForToolsMenuButton:YES];
}

- (void)toolsMenuWillHideNotification:(NSNotification*)note {
  [self.toolbarController setToolsMenuIsVisibleForToolsMenuButton:NO];
}

#pragma mark - Toolbar Commands

- (void)contractToolbar {
  [self.toolbarController cancelOmniboxEdit];
}

#pragma mark - Fullscreen helpers

// Creates a FullscreenUIUpdater for the toolbar controller and adds it as a
// FullscreenControllerObserver.
- (void)startObservingFullscreen {
  DCHECK(base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen));
  if (_fullscreenUpdater)
    return;
  if (!self.browserState)
    return;
  FullscreenController* fullscreenController =
      FullscreenControllerFactory::GetInstance()->GetForBrowserState(
          self.browserState);
  DCHECK(fullscreenController);
  _fullscreenUpdater =
      std::make_unique<FullscreenUIUpdater>(self.toolbarController);
  fullscreenController->AddObserver(_fullscreenUpdater.get());
}

// Removes the FullscreenUIUpdater as a FullscreenControllerObserver.
- (void)stopObservingFullscreen {
  DCHECK(base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen));
  if (!_fullscreenUpdater)
    return;
  if (!self.browserState)
    return;
  FullscreenController* fullscreenController =
      FullscreenControllerFactory::GetInstance()->GetForBrowserState(
          self.browserState);
  DCHECK(fullscreenController);
  fullscreenController->RemoveObserver(_fullscreenUpdater.get());
  _fullscreenUpdater = nullptr;
}

@end
