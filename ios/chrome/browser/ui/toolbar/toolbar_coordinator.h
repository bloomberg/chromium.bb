// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/omnibox_focuser.h"

@protocol ActivityServicePositioner;
@protocol QRScannerResultLoading;
@protocol TabHistoryPositioner;
@protocol TabHistoryUIUpdater;
@class TabModel;
@class ToolbarController;
@class ToolsMenuConfiguration;
@class ToolsPopupController;
@protocol VoiceSearchControllerDelegate;
@class WebToolbarController;
@protocol WebToolbarDelegate;

@interface LegacyToolbarCoordinator
    : ChromeCoordinator<BubbleViewAnchorPointProvider,
                        IncognitoViewControllerDelegate,
                        OmniboxFocuser>

@property(nonatomic, weak) TabModel* tabModel;
@property(nonatomic, readonly, strong) UIView* view;
// TODO(crbug.com/778226): Remove this property.
@property(nonatomic, strong) WebToolbarController* webToolbarController;

// Returns the different protocols and superclass now implemented by the
// WebToolbarController to avoid using the toolbar directly.
- (ToolbarController*)toolbarController;
- (id<VoiceSearchControllerDelegate>)voiceSearchDelegate;
- (id<ActivityServicePositioner>)activityServicePositioner;
- (id<TabHistoryPositioner>)tabHistoryPositioner;
- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater;
- (id<QRScannerResultLoading>)QRScannerResultLoader;

// Sets the WebToolbarController for this coordinator.
- (void)setWebToolbar:(WebToolbarController*)webToolbarController;

// Sets the delegate for the toolbar.
- (void)setToolbarDelegate:(id<WebToolbarDelegate>)delegate;

// Sets the height of the toolbar to be the .
- (void)adjustToolbarHeight;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
