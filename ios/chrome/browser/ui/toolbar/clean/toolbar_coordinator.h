// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_presentation_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"

@protocol ActivityServicePositioner;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ToolbarButtonUpdater;
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
                                         ToolsMenuPresentationProvider,
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
// Button updater for the toolbar.
@property(nonatomic, strong) ToolbarButtonUpdater* buttonUpdater;

// Returns the ActivityServicePositioner for this toolbar.
- (id<ActivityServicePositioner>)activityServicePositioner;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

// TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
// created.
// Updates the visibility of the Omnibox text and Toolbar buttons.
- (void)updateToolbarState;
// Updates the toolbar so it is in a state where a snapshot for |webState| can
// be taken.
- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState;
// Resets the toolbar after taking a side swipe snapshot. After calling this
// method the toolbar is adapted to the current webState.
- (void)resetToolbarAfterSideSwipeSnapshot;
// Sets the ToolsMenu visibility depending if the tools menu |isVisible|.
- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible;
// Triggers the animation of the tools menu button.
- (void)triggerToolsMenuButtonAnimation;
// Sets the background color of the Toolbar to the one of the Incognito NTP,
// with an |alpha|.
- (void)setBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha;
// Briefly animate the progress bar when a pre-rendered tab is displayed.
- (void)showPrerenderingAnimation;
// Returns whether omnibox is a first responder.
- (BOOL)isOmniboxFirstResponder;
// Returns whether the omnibox popup is currently displayed.
- (BOOL)showingOmniboxPopup;
// Activates constraints to simulate a safe area with |fakeSafeAreaInsets|
// insets. The insets will be used as leading/trailing wrt RTL. Those
// constraints have a higher priority than the one used to respect the safe
// area. They need to be deactivated for the toolbar to respect the safe area
// again. The fake safe area can be bigger or smaller than the real safe area.
- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets;
// Deactivates the constraints used to create a fake safe area.
- (void)deactivateFakeSafeAreaInsets;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_COORDINATOR_H_
