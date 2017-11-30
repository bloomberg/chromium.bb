// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller_delegate.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_toolbar_interacting.h"
#import "ios/chrome/browser/ui/toolbar/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/abstract_web_toolbar.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_provider.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_state_provider.h"

@protocol ActivityServicePositioner;
@protocol QRScannerResultLoading;
@class Tab;
@protocol TabHistoryPositioner;
@protocol TabHistoryUIUpdater;
@protocol VoiceSearchControllerDelegate;
@protocol WebToolbarDelegate;
@protocol ToolsMenuConfigurationProvider;

@class CommandDispatcher;
@class TabModel;
@class ToolbarController;
@class WebToolbarController;

@protocol Toolbar<AbstractWebToolbar,
                  OmniboxFocuser,
                  VoiceSearchControllerDelegate,
                  ActivityServicePositioner,
                  QRScannerResultLoading,
                  BubbleViewAnchorPointProvider,
                  ToolsMenuPresentationProvider>

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible;
- (void)start;

@end

@interface LegacyToolbarCoordinator
    : ChromeCoordinator<BubbleViewAnchorPointProvider,
                        IncognitoViewControllerDelegate,
                        OmniboxFocuser,
                        SideSwipeToolbarInteracting,
                        ToolbarSnapshotProviding,
                        ToolsMenuPresentationStateProvider>

@property(nonatomic, weak) TabModel* tabModel;
@property(nonatomic, strong) UIViewController* toolbarViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
            toolsMenuConfigurationProvider:
                (id<ToolsMenuConfigurationProvider>)configurationProvider
                                dispatcher:(CommandDispatcher*)dispatcher;

// Returns the different protocols and superclass now implemented by the
- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate;
- (id<ActivityServicePositioner>)activityServicePositioner;
- (id<TabHistoryPositioner>)tabHistoryPositioner;
- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater;
- (id<QRScannerResultLoading>)QRScannerResultLoader;

// Sets the toolbarController for this coordinator.
- (void)setToolbarController:(id<Toolbar>)toolbarController;

// Sets the delegate for the toolbar.
- (void)setToolbarDelegate:(id<WebToolbarDelegate>)delegate;

// TabModel callbacks.
- (void)selectedTabChanged;
- (void)setTabCount:(NSInteger)tabCount;

// WebToolbarController public interface.
- (void)browserStateDestroyed;
- (void)updateToolbarState;
- (void)setShareButtonEnabled:(BOOL)enabled;
- (void)showPrerenderingAnimation;
- (BOOL)isOmniboxFirstResponder;
- (BOOL)showingOmniboxPopup;
- (void)currentPageLoadStarted;
- (CGRect)visibleOmniboxFrame;
- (void)triggerToolsMenuButtonAnimation;
- (void)adjustToolbarHeight;
- (BOOL)isShowingToolsMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
