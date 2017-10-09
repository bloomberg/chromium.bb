// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/qr_scanner/requirements/qr_scanner_result_loading.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"
#include "ios/web/public/navigation_item_list.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class Tab;
@protocol ToolbarFrameDelegate;
class ToolbarModelIOS;
@protocol UrlLoader;

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// Notification when the tab history popup is shown.
extern NSString* const kTabHistoryPopupWillShowNotification;
// Notification when the tab history popup is hidden.
extern NSString* const kTabHistoryPopupWillHideNotification;
// The brightness of the omnibox placeholder text in regular mode,
// on an iPhone.
extern const CGFloat kiPhoneOmniboxPlaceholderColorBrightness;

// Delegate interface, to be implemented by the controller's delegate.
@protocol WebToolbarDelegate<NSObject>
@required
// Called when the location bar gains keyboard focus.
- (IBAction)locationBarDidBecomeFirstResponder:(id)sender;
// Called when the location bar loses keyboard focus.
- (IBAction)locationBarDidResignFirstResponder:(id)sender;
// Called when the location bar receives a key press.
- (IBAction)locationBarBeganEdit:(id)sender;
// Returns the WebState.
- (web::WebState*)currentWebState;
- (ToolbarModelIOS*)toolbarModelIOS;
@optional
// Called before the toolbar screenshot gets updated.
- (void)willUpdateToolbarSnapshot;
@end

// This protocol provides callbacks for focusing and blurring the omnibox.
@protocol OmniboxFocuser
// Give focus to the omnibox, if it is visible. No-op if it is not visible.
- (void)focusOmnibox;
// Cancel omnibox edit (from shield tap or cancel button tap).
- (void)cancelOmniboxEdit;
// Give focus to the omnibox, but indicate that the focus event was initiated
// from the fakebox on the Google landing page.
- (void)focusFakebox;
// Hides the toolbar when the fakebox is blurred.
- (void)onFakeboxBlur;
// Shows the toolbar when the fakebox has animated to full bleed.
- (void)onFakeboxAnimationComplete;
@end

// Web-view specific toolbar, adding navigation controls like back/forward,
// omnibox, etc.
@interface WebToolbarController
    : ToolbarController<OmniboxFocuser,
                        QRScannerResultLoading,
                        TabHistoryPositioner,
                        TabHistoryUIUpdater,
                        VoiceSearchControllerDelegate>

@property(nonatomic, weak) id<WebToolbarDelegate> delegate;
@property(nonatomic, weak, readonly) id<UrlLoader> urlLoader;

// Mark inherited initializer as unavailable.
- (instancetype)initWithStyle:(ToolbarControllerStyle)style
                   dispatcher:(id<BrowserCommands>)dispatcher NS_UNAVAILABLE;

// Create a new web toolbar controller whose omnibox is backed by
// |browserState|.
- (instancetype)initWithDelegate:(id<WebToolbarDelegate>)delegate
                       urlLoader:(id<UrlLoader>)urlLoader
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

// Called when the browser state this object was initialized with is being
// destroyed.
- (void)browserStateDestroyed;

// Update the visibility of the back/forward buttons, omnibox, etc.
- (void)updateToolbarState;

// Update the visibility of the toolbar before making a side swipe snapshot so
// the toolbar looks appropriate for |tab|. This includes morphing the toolbar
// to look like the new tab page header.
- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab;

// Remove any formatting added by -updateToolbarForSideSwipeSnapshot.
- (void)resetToolbarAfterSideSwipeSnapshot;

// Briefly animate the progress bar when a pre-rendered tab is displayed.
- (void)showPrerenderingAnimation;

// Called when the current page starts loading.
- (void)currentPageLoadStarted;

// Called when the current tab changes or is closed.
- (void)selectedTabChanged;

// Returns visible omnibox frame in WebToolbarController's view coordinate
// system.
- (CGRect)visibleOmniboxFrame;

// Returns whether omnibox is a first responder.
- (BOOL)isOmniboxFirstResponder;

// Returns whether the omnibox popup is currently displayed.
- (BOOL)showingOmniboxPopup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
