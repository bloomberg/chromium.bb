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

@class TabModel;
@class ToolsMenuConfiguration;
@class ToolsPopupController;
@class WebToolbarController;

@interface LegacyToolbarCoordinator
    : ChromeCoordinator<BubbleViewAnchorPointProvider,
                        IncognitoViewControllerDelegate,
                        OmniboxFocuser>

@property(nonatomic, weak) TabModel* tabModel;
@property(nonatomic, readonly, strong) UIView* view;
@property(nonatomic, strong) WebToolbarController* webToolbarController;

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
