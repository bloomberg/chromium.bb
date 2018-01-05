// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/legacy_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_state_provider.h"

@class CommandDispatcher;
@protocol ToolsMenuConfigurationProvider;

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

// Sets the toolbarController for this coordinator.
- (void)setToolbarController:(id<Toolbar>)toolbarController;

// ToolbarController public interface.
- (void)updateToolbarState;
- (void)triggerToolsMenuButtonAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_LEGACY_TOOLBAR_COORDINATOR_H_
