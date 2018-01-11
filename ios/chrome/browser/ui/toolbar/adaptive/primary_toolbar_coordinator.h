// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/primary_toolbar_coordinator.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ToolbarCommands;
@protocol ToolbarCoordinatorDelegate;
@protocol ToolsMenuConfigurationProvider;
@protocol UrlLoader;
class WebStateList;

// Coordinator for the primary toolbar. In an adaptive toolbar paradigm, this is
// the toolbar always displayed.
@interface PrimaryToolbarCoordinator
    : ChromeCoordinator<PrimaryToolbarCoordinator, ToolbarCommands>

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, ToolbarCommands>
        dispatcher;
// The web state list this ToolbarCoordinator is handling.
@property(nonatomic, assign) WebStateList* webStateList;
// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;
// URL loader for the toolbar.
// TODO(crbug.com/799446): Remove this.
@property(nonatomic, weak) id<UrlLoader> URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_COORDINATOR_H_
