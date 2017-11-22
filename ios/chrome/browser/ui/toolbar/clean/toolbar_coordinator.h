// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/omnibox_focuser.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ToolbarCoordinatorDelegate;
@protocol UrlLoader;
class WebStateList;
namespace ios {
class ChromeBrowserState;
}
namespace web {
class WebState;
}

// Coordinator to run a toolbar -- a UI element housing controls.
@interface ToolbarCoordinator : NSObject<OmniboxFocuser,
                                         QRScannerResultLoading,
                                         VoiceSearchControllerDelegate>

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
// UIViewController managed by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

// Updates the toolbar so it is in a state where a snapshot for |webState| can
// be taken.
- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState;
// Resets the toolbar after taking a side swipe snapshot. After calling this
// method the toolbar is adapted to the current webState.
- (void)resetToolbarAfterSideSwipeSnapshot;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
