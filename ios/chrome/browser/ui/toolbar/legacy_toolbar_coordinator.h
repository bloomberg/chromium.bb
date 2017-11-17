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

@protocol ActivityServicePositioner;
@protocol QRScannerResultLoading;
@class Tab;
@protocol TabHistoryPositioner;
@protocol TabHistoryUIUpdater;
@class TabModel;
@class ToolbarController;
@class ToolsMenuConfiguration;
@class ToolsPopupController;
@protocol VoiceSearchControllerDelegate;
@class WebToolbarController;
@protocol WebToolbarDelegate;

@protocol Toolbar<AbstractWebToolbar,
                  OmniboxFocuser,
                  VoiceSearchControllerDelegate,
                  ActivityServicePositioner,
                  QRScannerResultLoading,
                  BubbleViewAnchorPointProvider>
@end

@interface LegacyToolbarCoordinator
    : ChromeCoordinator<BubbleViewAnchorPointProvider,
                        IncognitoViewControllerDelegate,
                        OmniboxFocuser,
                        SideSwipeToolbarInteracting,
                        ToolbarSnapshotProviding>

@property(nonatomic, weak) TabModel* tabModel;
@property(nonatomic, strong) UIViewController* toolbarViewController;

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
- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration;
- (ToolsPopupController*)toolsPopupController;
- (void)dismissToolsMenuPopup;
- (CGRect)visibleOmniboxFrame;
- (void)triggerToolsMenuButtonAnimation;
- (void)adjustToolbarHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
