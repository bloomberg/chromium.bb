// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ToolbarCoordinatorDelegate;
@protocol UrlLoader;
class WebStateList;
namespace ios {
class ChromeBrowserState;
}

// Coordinator to run a toolbar -- a UI element housing controls.
@interface ToolbarCoordinator : NSObject

// Weak reference to ChromeBrowserState;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The dispatcher for this view controller.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;
// The web state list this ToolbarCoordinator is handling.
@property(nonatomic, assign) WebStateList* webStateList;
// Delegate for this coordinator.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;
// URL loader for the toolbar.
@property(nonatomic, weak) id<UrlLoader> URLLoader;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
