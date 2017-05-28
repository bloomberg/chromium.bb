// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator.h"

@class ContextMenuContextImpl;

// A coordinator for a UI element that displays the web view associated with
// |webState|.
// HACK: Named WebContentMenuCoordinator to avoid collision with the
// old architecture ContextMenuCoordinator class.
@interface WebContextMenuCoordinator : BrowserCoordinator

// Initializer that takes the parameters used to specify which context menu
// options to display.
- (instancetype)initWithContext:(ContextMenuContextImpl*)context
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
