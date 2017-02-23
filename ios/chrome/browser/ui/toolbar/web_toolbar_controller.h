// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/omnibox/location_bar_view_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller_delegate.h"

@protocol PreloadProvider;
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
// Called when the stack view controller is about to be shown.
- (IBAction)prepareToEnterTabSwitcher:(id)sender;
// Loads the text entered in the location bar as javascript.
// Note: The JavaScript is executed asynchronously.
- (void)loadJavaScriptFromLocationBar:(NSString*)script;
// Returns the WebState.
- (web::WebState*)currentWebState;
// Called when the toolbar height changes. Other elements, such as the web view,
// may need to adjust accordingly. This is called from within an animation
// block.
- (void)toolbarHeightChanged;
- (ToolbarModelIOS*)toolbarModelIOS;
// Sets the alpha for the toolbar's background views.
- (void)updateToolbarBackgroundAlpha:(CGFloat)backgroundAlpha;
// Sets the alpha for the toolbar's background views.
- (void)updateToolbarControlsAlpha:(CGFloat)controlsAlpha;
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
                        QRScannerViewControllerDelegate,
                        VoiceSearchControllerDelegate>

@property(nonatomic, assign) id<WebToolbarDelegate> delegate;
@property(nonatomic, assign, readonly) id<UrlLoader> urlLoader;

// Mark inherited initializer as unavailable.
- (instancetype)initWithStyle:(ToolbarControllerStyle)style NS_UNAVAILABLE;

// Create a new web toolbar controller whose omnibox is backed by
// |browserState|.
- (instancetype)initWithDelegate:(id<WebToolbarDelegate>)delegate
                       urlLoader:(id<UrlLoader>)urlLoader
                    browserState:(ios::ChromeBrowserState*)browserState
                 preloadProvider:(id<PreloadProvider>)preloader
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

// Hides or shows the toolbar controls. When controls are hidden, the toolbar
// will be drawn as an empty bar with the usual background.
- (void)setControlsHidden:(BOOL)hidden;

// Set the alpha of the toolbar controls, for fading while tracking gestures.
- (void)setControlsAlpha:(CGFloat)alpha;

// Called when the current page starts loading.
- (void)currentPageLoadStarted;

// Called when the current tab changes or is closed.
- (void)selectedTabChanged;

// Returns the bound of the bookmark button. Used to position the bookmark
// editor.
- (CGRect)bookmarkButtonAnchorRect;

// Returns the bookmark button's view. Used to position the bookmark editor.
- (UIView*)bookmarkButtonView;

// Returns visible omnibox frame in WebToolbarController's view coordinate
// system.
- (CGRect)visibleOmniboxFrame;

// Returns a UIImage containing a snapshot of the view at the given width.  If
// |width| is 0, it uses the view's current width. Returns the cached snapshot
// if it is up to date.
- (UIImage*)snapshotWithWidth:(CGFloat)width;

// Shows the tab history popup inside |view|.
- (void)showTabHistoryPopupInView:(UIView*)view
               withSessionEntries:(NSArray*)sessionEntries
                   forBackHistory:(BOOL)isBackHistory;

// Dismisses the tab history popup.
- (void)dismissTabHistoryPopup;

// Returns whether omnibox is a first responder.
- (BOOL)isOmniboxFirstResponder;

// Returns whether the omnibox popup is currently displayed.
- (BOOL)showingOmniboxPopup;

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_H_
