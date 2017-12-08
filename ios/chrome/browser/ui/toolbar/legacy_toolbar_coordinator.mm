// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/legacy_toolbar_coordinator.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_coordinator.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyToolbarCoordinator () {
  // Coordinator for the tools menu UI.
  ToolsMenuCoordinator* _toolsMenuCoordinator;
  // The fullscreen updater.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUpdater;
}

@property(nonatomic, strong) id<Toolbar> toolbarController;
@end

@implementation LegacyToolbarCoordinator
@synthesize tabModel = _tabModel;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize toolbarController = _toolbarController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
            toolsMenuConfigurationProvider:
                (id<ToolsMenuConfigurationProvider>)configurationProvider
                                dispatcher:(CommandDispatcher*)dispatcher {
  if (self = [super initWithBaseViewController:viewController]) {
    _toolsMenuCoordinator = [[ToolsMenuCoordinator alloc]
        initWithBaseViewController:viewController];
    _toolsMenuCoordinator.dispatcher = dispatcher;
    _toolsMenuCoordinator.configurationProvider = configurationProvider;

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
  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen)) {
    _fullscreenUpdater =
        base::MakeUnique<FullscreenUIUpdater>(self.toolbarController);
    FullscreenControllerFactory::GetInstance()
        ->GetForBrowserState(self.tabModel.browserState)
        ->AddObserver(_fullscreenUpdater.get());
  }
}

- (void)stop {
  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen)) {
    FullscreenControllerFactory::GetInstance()
        ->GetForBrowserState(self.tabModel.browserState)
        ->RemoveObserver(_fullscreenUpdater.get());
    _fullscreenUpdater = nullptr;
  }
  self.toolbarController = nil;
}

- (UIViewController*)toolbarViewController {
  if (!_toolbarViewController)
    _toolbarViewController = self.toolbarController.viewController;

  return _toolbarViewController;
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

- (void)setToolbarController:(id<Toolbar>)toolbarController {
  _toolbarController = toolbarController;
  // ToolbarController needs to know about whether the tools menu is presented
  // or not, and does so by storing a reference to the coordinator to query.
  _toolsMenuCoordinator.presentationProvider = _toolbarController;
  [toolbarController start];
}

- (void)setToolbarDelegate:(id<WebToolbarDelegate>)delegate {
  self.toolbarController.delegate = delegate;
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
  [self.toolbarController setBackgroundAlpha:1.0];
  [self.toolbarController browserStateDestroyed];
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
  CGRect oldFrame = self.toolbarViewController.view.superview.frame;
  CGRect newFrame = oldFrame;
  newFrame.size.width = width;

  self.toolbarViewController.view.superview.frame = newFrame;
  [self.toolbarController activateFakeSafeAreaInsets:safeAreaInsets];
  [self.toolbarViewController.view.superview layoutIfNeeded];

  UIView* toolbarSnapshotView = [self snapshotForTabSwitcher];

  self.toolbarViewController.view.superview.frame = oldFrame;
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
  [self cancelOmniboxEdit];
}

@end
