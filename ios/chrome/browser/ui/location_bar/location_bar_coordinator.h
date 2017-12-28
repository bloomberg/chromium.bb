// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/location_bar/location_bar_view.h"

namespace ios {
class ChromeBrowserState;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;

@interface LocationBarCoordinator : NSObject

// LocationBarView containing the omnibox.
@property(nonatomic, strong) LocationBarView* locationBarView;
// Weak reference to ChromeBrowserState;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The dispatcher for this view controller.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
