// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/abstract_web_toolbar.h"
#import "ios/chrome/browser/ui/toolbar/public/legacy_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_provider.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_state_provider.h"

@protocol ActivityServicePositioner;
@class Tab;
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
                  ToolsMenuPresentationProvider,
                  FullscreenUIElement>

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible;
- (void)start;
- (void)stop;

@end

@interface LegacyToolbarCoordinator
    : ChromeCoordinator<PrimaryToolbarCoordinator,
                        IncognitoViewControllerDelegate,
                        LegacyToolbarCoordinator,
                        ToolbarSnapshotProviding,
                        ToolsMenuPresentationStateProvider>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
            toolsMenuConfigurationProvider:
                (id<ToolsMenuConfigurationProvider>)configurationProvider
                                dispatcher:(CommandDispatcher*)dispatcher
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// Returns the different protocols and superclass now implemented by the
// internal ViewController.
- (id<ActivityServicePositioner>)activityServicePositioner;

// Sets the toolbarController for this coordinator.
- (void)setToolbarController:(id<Toolbar>)toolbarController;

// ToolbarController public interface.
- (void)updateToolbarState;
- (CGRect)visibleOmniboxFrame;
- (void)triggerToolsMenuButtonAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
