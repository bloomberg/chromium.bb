// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_ADAPTOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_ADAPTOR_H_

#import "ios/chrome/browser/ui/toolbar/public/toolbar_coordinating.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_state_provider.h"

@class CommandDispatcher;
@protocol ToolbarCoordinatee;

// This object is an interface between multiple toolbars and the objects which
// want to interact with them without having to know to which one specifically
// send the call.
@interface ToolbarCoordinatorAdaptor : NSObject<ToolbarCoordinating>

- (instancetype)initWithDispatcher:(CommandDispatcher*)dispatcher;

// Adds a |toolbarCoordinator| to the set of coordinators this object is
// interfacing with.
- (void)addToolbarCoordinator:(id<ToolbarCoordinatee>)toolbarCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_ADAPTOR_H_
