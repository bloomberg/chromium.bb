// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_view.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"

namespace ios {
class ChromeBrowserState;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol UrlLoader;
@protocol ToolbarCoordinatorDelegate;
class LocationBarControllerImpl;

@interface LocationBarCoordinator
    : NSObject<LocationBarURLLoader, OmniboxFocuser>

// LocationBarView containing the omnibox.
@property(nonatomic, strong) LocationBarView* locationBarView;
// Weak reference to ChromeBrowserState;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The dispatcher for this view controller.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;
// URL loader for the location bar.
@property(nonatomic, weak) id<UrlLoader> URLLoader;
// The location bar controller.
// TODO: this class needs to own this instance instead of ToolbarCoordinator.
@property(nonatomic, assign) LocationBarControllerImpl* locationBarController;
// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

// Updates omnibox state, including the displayed text and the cursor position.
- (void)updateOmniboxState;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
