// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/net/crw_request_tracker_delegate.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#import "ios/web/web_state/ui/crw_touch_tracking_recognizer.h"

namespace web {

// Page load phases.
enum LoadPhase {
  // In the LOAD_REQUESTED phase, the system predicts a page change is going to
  // happen but the page URL has not yet changed.
  LOAD_REQUESTED = 0,
  // In the PAGE_LOADING phase, the page URL has changed but the whole document
  // may not be available for use.
  PAGE_LOADING = 1,
  // In the PAGE_LOADED phase, either the page had loaded and is available for
  // use, the load was cancelled, or the UIWebView is new and ready for a load.
  PAGE_LOADED = 2
};

}  // namespace web

@class CRWJSInjectionReceiver;
@protocol CRWNativeContent;
@protocol CRWNativeContentProvider;
@protocol CRWSwipeRecognizerProvider;
@protocol CRWWebControllerObserver;
@class CRWWebViewContentView;
@protocol CRWWebViewProxy;
class GURL;

namespace web {
class WebState;
class WebStateImpl;
}

// Manages a view that can be used either for rendering web content in a web
// view, or native content in a view provided by a NativeContentProvider.
// CRWWebController also transparently evicts and restores the internal web
// view based on memory pressure, and manages access to interact with the
// web view.
// This is an abstract class which must not be instantiated directly.
// TODO(stuartmorgan): Move all of the navigation APIs out of this class.
@interface CRWWebController : NSObject<CRWJSInjectionEvaluator,
                                       CRWTouchTrackingDelegate,
                                       UIGestureRecognizerDelegate>

// Whether or not a UIWebView is allowed to exist in this CRWWebController.
// Defaults to NO; this should be enabled before attempting to access the view.
@property(nonatomic, assign) BOOL webUsageEnabled;

@property(nonatomic, assign) id<CRWWebDelegate> delegate;
@property(nonatomic, assign) id<CRWNativeContentProvider> nativeProvider;
@property(nonatomic, assign)
    id<CRWSwipeRecognizerProvider> swipeRecognizerProvider;
@property(nonatomic, readonly) web::WebState* webState;
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;

// The container view used to display content.  If the view has been purged due
// to low memory, this will recreate it.
@property(nonatomic, readonly) UIView* view;

// The web view proxy associated with this controller.
@property(nonatomic, readonly) id<CRWWebViewProxy> webViewProxy;

// The view that generates print data when printing. It is nil if printing
// is not supported.
@property(nonatomic, readonly) UIView* viewForPrinting;

// Content view was reset due to low memory.  Use placeholder overlay view on
// next creation.
@property(nonatomic, readwrite, assign) BOOL usePlaceholderOverlay;

// Returns the current page loading phase.
@property(nonatomic, readonly) web::LoadPhase loadPhase;

// The fraction of the page load that has completed as a number between 0.0
// (nothing loaded) and 1.0 (fully loaded).
@property(nonatomic, readonly) double loadingProgress;

// Returns the x, y offset the content has been scrolled.
@property(nonatomic, readonly) CGPoint scrollPosition;

// Returns whether the top of the content is visible.
@property(nonatomic, readonly) BOOL atTop;

// YES if JavaScript dialogs, HTTP authentication dialogs and window.open
// calls should be suppressed. Default is NO. When dialog is suppressed
// |CRWWebDelegate webControllerDidSuppressDialog:| will be called.
@property(nonatomic, assign) BOOL shouldSuppressDialogs;

// Designated initializer. Initializes web controller with |webState|. The
// calling code must retain the ownership of |webState|.
- (instancetype)initWithWebState:(web::WebStateImpl*)webState;

// Return an image to use as replacement of a missing snapshot.
+ (UIImage*)defaultSnapshotImage;

// Replaces the currently displayed content with |contentView|.  The content
// view will be dismissed for the next navigation.
- (void)showTransientContentView:(CRWContentView*)contentView;

// Clear the transient content view, if one is shown.
- (void)clearTransientContentView;

// Call to stop the CRWWebController from doing stuff, in particular to
// stop all network requests. Called as part of the close sequence if it hasn't
// already been halted; also called from [Tab halt] as part of the shutdown
// sequence (which doesn't call -close).
- (void)terminateNetworkActivity;

// Dismisses all modals owned by the web view or native view.
- (void)dismissModals;

// Call when the CRWWebController needs go away. Caller must reset the delegate
// before calling.
- (void)close;

// Call when there is a need to free up memory.
- (void)handleLowMemory;

// Returns YES if there is currently a live view in the tab (e.g., the view
// hasn't been discarded due to low memory).
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care whether the view is live.
- (BOOL)isViewAlive;

// Returns YES if the current live view is a web view with HTML.
- (BOOL)contentIsHTML;

// Returns the CRWWebController's view of the current URL. Moreover, this method
// will set the trustLevel enum to the appropriate level from a security point
// of view. The caller has to handle the case where |trustLevel| is not
// appropriate, as this method won't display any error to the user.
- (GURL)currentURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel;

// Methods for navigation and properties to interrogate state.
- (void)reload;
- (void)stopLoading;
// YES if the CRWWebController's view is deemed appropriate for saving in order
// to generate an overlay placeholder view.
- (BOOL)canUseViewForGeneratingOverlayPlaceholderView;

// Start loading the URL specified in |originalParams|, with the specified
// settings.  Always resets the openedByScript property to NO.
- (void)loadWithParams:(const web::NavigationManager::WebLoadParams&)params;

// Loads the URL indicated by current session state.
- (void)loadCurrentURL;

// Loads HTML in the page and presents it as if it was originating from an
// application specific URL.
- (void)loadHTML:(NSString*)HTML forAppSpecificURL:(const GURL&)URL;

// Loads HTML in the page and presents it as if it was originating from the
// URL itself. Should be used only in specific cases, where the injected html
// is guaranteed to be some derived representation of the original content.
- (void)loadHTMLForCurrentURL:(NSString*)HTML;

// Stops loading the page.
- (void)stopLoading;

// Causes the page to start loading immediately if there is a pending load;
// normally if the web view has been paged out for memory reasons, loads are
// started lazily the next time the view is displayed. This can be called to
// bypass the lazy behavior. This is equivalent to calling -view, but should be
// used when deliberately pre-triggering a load without displaying.
- (void)triggerPendingLoad;

// Navigates to the item at the given |index|.
- (void)goToItemAtIndex:(int)index;

// Executes |script| in the web view, registering user interaction.
- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(web::JavaScriptResultBlock)completion;

// Dismisses the soft keyboard.
- (void)dismissKeyboard;

// Requires that the next load rebuild the UIWebView. This is expensive, and
// should be used only in the case where something has changed that UIWebView
// only checks on creation, such that the whole object needs to be rebuilt.
// TODO(stuartmorgan): Merge this and reinitializeWebViewAndReload:. They are
// currently subtly different in terms of implementation, but are for
// fundamentally the same purpose.
- (void)requirePageReconstruction;

- (void)reinitializeWebViewAndReload:(BOOL)reload;

// Requires that the next display reload the page, using a placeholder while
// loading. This could be used, e.g., to handle a crash in a WebController that
// is not currently visible.
// TODO(stuartmorgan): When revisiting the methods above, revisit this as well.
- (void)requirePageReload;

// Show overlay, don't reload web page. Used when the view will be
// visible only briefly (e.g., tablet side swipe).
- (void)setOverlayPreviewMode:(BOOL)overlayPreviewMode;

// Records the state (scroll position, form values, whatever can be harvested)
// from the current page into the current session entry.
- (void)recordStateInHistory;
// Restores the state for this page from session history.
// TODO(stuartmorgan): This is public only temporarily; once refactoring is
// complete it will be handled internally.
- (void)restoreStateFromHistory;

// Notifies the CRWWebController that it has been shown.
- (void)wasShown;

// Notifies the CRWWebController that the current page is an HTTP page
// containing a password field.
- (void)didShowPasswordInputOnHTTP;

// Notifies the CRWWebController that the current page is an HTTP page
// containing a credit card field.
- (void)didShowCreditCardInputOnHTTP;

// Notifies the CRWWebController that it has been hidden.
- (void)wasHidden;

// Returns |YES| if the current page should show the keyboard shield.
- (BOOL)wantsKeyboardShield;

// Returns |YES| if the current page should should the location bar hint text.
- (BOOL)wantsLocationBarHintText;

// Adds |recognizer| as a gesture recognizer to the web view.
- (void)addGestureRecognizerToWebView:(UIGestureRecognizer*)recognizer;
// Removes |recognizer| from the web view.
- (void)removeGestureRecognizerFromWebView:(UIGestureRecognizer*)recognizer;

// Adds |toolbar| to the web view.
- (void)addToolbarViewToWebView:(UIView*)toolbarView;
// Removes |toolbar| from the web view.
- (void)removeToolbarViewFromWebView:(UIView*)toolbarView;

// Adds a CRWWebControllerObserver to subscribe to page events. |observer|
// cannot be nil.
- (void)addObserver:(id<CRWWebControllerObserver>)observer;

// Removes an attached CRWWebControllerObserver.
- (void)removeObserver:(id<CRWWebControllerObserver>)observer;

// Returns the always-visible frame, not including the part that could be
// covered by the toolbar.
- (CGRect)visibleFrame;

- (CRWJSInjectionReceiver*)jsInjectionReceiver;

// Load the correct HTML page for |error| in a native controller, retrieved
// from the native provider. Call |loadNativeViewWithSuccess:NO| to load the
// native controller.
- (void)loadErrorInNativeView:(NSError*)error;

// Returns the native controller (if any) current mananging the content.
- (id<CRWNativeContent>)nativeController;
@end

#pragma mark Testing

@interface CRWWebController (UsedOnlyForTesting)  // Testing or internal API.

// YES if a user interaction has been registered at any time since the page has
// loaded.
@property(nonatomic, readwrite) BOOL userInteractionRegistered;
// Returns whether the user is interacting with the page.
@property(nonatomic, readonly) BOOL userIsInteracting;

// Injects a CRWWebViewContentView for testing.  Takes ownership of
// |webViewContentView|.
- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView;
- (void)resetInjectedWebViewContentView;
// Returns the number of observers registered for this CRWWebController.
- (NSUInteger)observerCount;
- (void)setURLOnStartLoading:(const GURL&)url;
- (void)simulateLoadRequestWithURL:(const GURL&)URL;

// Returns the header height.
- (CGFloat)headerHeight;

// Loads the HTML into the page at the given URL.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL;

// Caches request POST data in the given session entry.  Exposed for testing.
- (void)cachePOSTDataForRequest:(NSURLRequest*)request
                 inSessionEntry:(CRWSessionEntry*)currentSessionEntry;

// Acts on a single message from the JS object, parsed from JSON into a
// DictionaryValue. Returns NO if the format for the message was invalid.
- (BOOL)respondToMessage:(base::DictionaryValue*)crwMessage
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_H_
