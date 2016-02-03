// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <objc/runtime.h>
#include <stddef.h>

#include <cmath>
#include <utility>

#include "base/ios/block_types.h"
#import "base/ios/ns_error_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/net/nsurlrequest_util.h"
#include "ios/public/provider/web/web_ui_ios.h"
#import "ios/web/history_state_util.h"
#include "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/web_load_params.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon_url.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/referrer_util.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/user_metrics.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_kit_constants.h"
#include "ios/web/public/web_state/credential.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/credential_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/error_translation_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_early_script_manager.h"
#import "ios/web/web_state/js/crw_js_plugin_placeholder_manager.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/ui/crw_context_menu_provider.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#import "ios/web/web_state/ui/crw_ui_web_view_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"
#import "ios/web/web_state/web_controller_observer_bridge.h"
#include "ios/web/web_state/web_state_facade_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#import "ui/base/ios/cru_context_menu_holder.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::UserMetricsAction;
using web::NavigationManagerImpl;
using web::WebState;
using web::WebStateImpl;

namespace web {

NSString* const kContainerViewID = @"Container View";
const char* kWindowNameSeparator = "#";
NSString* const kUserIsInteractingKey = @"userIsInteracting";
NSString* const kOriginURLKey = @"originURL";
NSString* const kLogJavaScript = @"LogJavascript";

NewWindowInfo::NewWindowInfo(GURL target_url,
                             NSString* target_window_name,
                             web::ReferrerPolicy target_referrer_policy,
                             bool target_user_is_interacting)
    : url(target_url),
      window_name([target_window_name copy]),
      referrer_policy(target_referrer_policy),
      user_is_interacting(target_user_is_interacting) {
}

NewWindowInfo::~NewWindowInfo() {
}

// Struct to capture data about a user interaction. Records the time of the
// interaction and the main document URL at that time.
struct UserInteractionEvent {
  UserInteractionEvent(GURL url)
      : main_document_url(url), time(CFAbsoluteTimeGetCurrent()) {}
  // Main document URL at the time the interaction occurred.
  GURL main_document_url;
  // Time that the interaction occured, measured in seconds since Jan 1 2001.
  CFAbsoluteTime time;
};

}  // namespace web

namespace {

// A tag for the web view, so that tests can identify it. This is used instead
// of exposing a getter (and deliberately not exposed in the header) to make it
// *very* clear that this is a hack which should only be used as a last resort.
const NSUInteger kWebViewTag = 0x3eb71e3;

// States for external URL requests. This enum is used in UMA and
// entries should not be re-ordered or deleted.
enum ExternalURLRequestStatus {
  MAIN_FRAME_ALLOWED = 0,
  SUBFRAME_ALLOWED,
  SUBFRAME_BLOCKED,
  NUM_EXTERNAL_URL_REQUEST_STATUS
};

// Cancels touch events for the given gesture recognizer.
void CancelTouches(UIGestureRecognizer* gesture_recognizer) {
  if (gesture_recognizer.enabled) {
    gesture_recognizer.enabled = NO;
    gesture_recognizer.enabled = YES;
  }
}

// Cancels all touch events for web view (long presses, tapping, scrolling).
void CancelAllTouches(UIScrollView* web_scroll_view) {
  // Disable web view scrolling.
  CancelTouches(web_scroll_view.panGestureRecognizer);

  // All user gestures are handled by a subview of web view scroll view
  // (UIWebBrowserView for UIWebView and WKContentView for WKWebView).
  for (UIView* subview in web_scroll_view.subviews) {
    for (UIGestureRecognizer* recognizer in subview.gestureRecognizers) {
      CancelTouches(recognizer);
    }
  }
}

}  // namespace

@interface CRWWebController () <CRWNativeContentDelegate,
                                CRWWebControllerContainerViewDelegate,
                                CRWWebViewScrollViewProxyObserver> {
  base::WeakNSProtocol<id<CRWWebDelegate>> _delegate;
  base::WeakNSProtocol<id<CRWWebUserInterfaceDelegate>> _UIDelegate;
  base::WeakNSProtocol<id<CRWNativeContentProvider>> _nativeProvider;
  base::WeakNSProtocol<id<CRWSwipeRecognizerProvider>> _swipeRecognizerProvider;
  // The CRWWebViewProxy is the wrapper to give components access to the
  // web view in a controlled and limited way.
  base::scoped_nsobject<CRWWebViewProxyImpl> _webViewProxy;
  // The view used to display content.  Must outlive |_webViewProxy|.
  base::scoped_nsobject<CRWWebControllerContainerView> _containerView;
  // If |_contentView| contains a native view rather than a web view, this
  // is its controller. If it's a web view, this is nil.
  base::scoped_nsprotocol<id<CRWNativeContent>> _nativeController;
  BOOL _isHalted;  // YES if halted. Halting happens prior to destruction.
  BOOL _isBeingDestroyed;  // YES if in the process of closing.
  // All CRWWebControllerObservers attached to the CRWWebController. A
  // specially-constructed set is used that does not retain its elements.
  base::scoped_nsobject<NSMutableSet> _observers;
  // Each observer in |_observers| is associated with a
  // WebControllerObserverBridge in order to listen from WebState callbacks.
  // TODO(droger): Remove |_observerBridges| when all CRWWebControllerObservers
  // are converted to WebStateObservers.
  ScopedVector<web::WebControllerObserverBridge> _observerBridges;
  // |windowId| that is saved when a page changes. Used to detect refreshes.
  base::scoped_nsobject<NSString> _lastSeenWindowID;
  // YES if a user interaction has been registered at any time once the page has
  // loaded.
  BOOL _userInteractionRegistered;
  // Last URL change reported to webWill/DidStartLoadingURL. Used to detect page
  // location changes (client redirects) in practice.
  GURL _lastRegisteredRequestURL;
  // Last URL change reported to webDidStartLoadingURL. Used to detect page
  // location changes in practice.
  GURL _URLOnStartLoading;
  // Page loading phase.
  web::LoadPhase _loadPhase;
  // The web::PageDisplayState recorded when the page starts loading.
  web::PageDisplayState _displayStateOnStartLoading;
  // Whether or not the page has zoomed since the current navigation has been
  // committed, either by user interaction or via |-restoreStateFromHistory|.
  BOOL _pageHasZoomed;
  // Whether a PageDisplayState is currently being applied.
  BOOL _applyingPageState;
  // Actions to execute once the page load is complete.
  base::scoped_nsobject<NSMutableArray> _pendingLoadCompleteActions;
  // UIGestureRecognizers to add to the web view.
  base::scoped_nsobject<NSMutableArray> _gestureRecognizers;
  // Toolbars to add to the web view.
  base::scoped_nsobject<NSMutableArray> _webViewToolbars;
  // Flag to say if browsing is enabled.
  BOOL _webUsageEnabled;
  // Content view was reset due to low memory. Use the placeholder overlay on
  // next creation.
  BOOL _usePlaceholderOverlay;
  // The next time the view is requested, reload the page (using the placeholder
  // overlay until it's loaded).
  BOOL _requireReloadOnDisplay;
  // Overlay view used instead of webView.
  base::scoped_nsobject<UIImageView> _placeholderOverlayView;
  // The touch tracking recognizer allowing us to decide if a navigation is
  // started by the user.
  base::scoped_nsobject<CRWTouchTrackingRecognizer> _touchTrackingRecognizer;
  // Long press recognizer that allows showing context menus.
  base::scoped_nsobject<UILongPressGestureRecognizer> _contextMenuRecognizer;
  // DOM element information for the point where the user made the last touch.
  // Can be null if has not been calculated yet. Precalculation is necessary
  // because retreiving DOM element relies on async API so element info can not
  // be built on demand. May contain the following keys: "href", "src", "title",
  // "referrerPolicy". All values are strings. Used for showing context menus.
  scoped_ptr<base::DictionaryValue> _DOMElementForLastTouch;
  // Whether a click is in progress.
  BOOL _clickInProgress;
  // Data on the recorded last user interaction.
  scoped_ptr<web::UserInteractionEvent> _lastUserInteraction;
  // YES if there has been user interaction with views owned by this controller.
  BOOL _userInteractedWithWebController;
  // The time of the last page transfer start, measured in seconds since Jan 1
  // 2001.
  CFAbsoluteTime _lastTransferTimeInSeconds;
  // Default URL (about:blank).
  GURL _defaultURL;
  // Show overlay view, don't reload web page.
  BOOL _overlayPreviewMode;
  // If |YES|, call setSuppressDialogs when core.js is injected into the web
  // view.
  BOOL _setSuppressDialogsLater;
  // If |YES|, call setSuppressDialogs when core.js is injected into the web
  // view.
  BOOL _setNotifyAboutDialogsLater;
  // The URL of an expected future recreation of the |webView|. Valid
  // only if the web view was discarded for non-user-visible reasons, such that
  // if the next load request is for that URL, it should be treated as a
  // reconstruction that should use cache aggressively.
  GURL _expectedReconstructionURL;

  scoped_ptr<web::NewWindowInfo> _externalRequest;

  // The WebStateImpl instance associated with this CRWWebController.
  scoped_ptr<WebStateImpl> _webStateImpl;

  // A set of URLs opened in external applications; stored so that errors
  // from the web view can be identified as resulting from these events.
  base::scoped_nsobject<NSMutableSet> _openedApplicationURL;

  // Object that manages all early script injection into the web view.
  base::scoped_nsobject<CRWJSEarlyScriptManager> _earlyScriptManager;

  // Script manager for setting the windowID.
  base::scoped_nsobject<CRWJSWindowIdManager> _windowIDJSManager;

  // The receiver of JavaScripts.
  base::scoped_nsobject<CRWJSInjectionReceiver> _jsInjectionReceiver;
}

// The container view.  The container view should be accessed through this
// property rather than |self.view| from within this class, as |self.view|
// triggers creation while |self.containerView| will return nil if the view
// hasn't been instantiated.
@property(nonatomic, retain, readonly)
    CRWWebControllerContainerView* containerView;
// The current page state of the web view. Writing to this property
// asynchronously applies the passed value to the current web view.
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
// The currently displayed native controller, if any.
@property(nonatomic, readwrite) id<CRWNativeContent> nativeController;
// Removes the container view from the hierarchy and resets the ivar.
- (void)resetContainerView;
// Resets any state that is associated with a specific document object (e.g.,
// page interaction tracking).
- (void)resetDocumentSpecificState;
// Returns YES if the URL looks like it is one CRWWebController can show.
+ (BOOL)webControllerCanShow:(const GURL&)url;
// Clears the currently-displayed transient content view.
- (void)clearTransientContentView;
// Returns a lazily created CRWTouchTrackingRecognizer.
- (CRWTouchTrackingRecognizer*)touchTrackingRecognizer;
// Shows placeholder overlay.
- (void)addPlaceholderOverlay;
// Removes placeholder overlay.
- (void)removePlaceholderOverlay;
// Returns |YES| if |url| should be loaded in a native view.
- (BOOL)shouldLoadURLInNativeView:(const GURL&)url;
// Loads the HTML into the page at the given URL.
- (void)loadHTML:(NSString*)html forURL:(const GURL&)url;
// Loads the current nativeController in a native view. If a web view is
// present, removes it and swaps in the native view in its place.
- (void)loadNativeViewWithSuccess:(BOOL)loadSuccess;
// YES if the navigation to |url| should be treated as a reload.
- (BOOL)shouldReload:(const GURL&)destinationURL
          transition:(ui::PageTransition)transition;
// Internal implementation of reload. Reloads without notifying the delegate.
// Most callers should use -reload instead.
- (void)reloadInternal;
// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad;
// Called after URL is finished loading and _loadPhase is set to PAGE_LOADED.
- (void)didFinishWithURL:(const GURL&)currentURL loadSuccess:(BOOL)loadSuccess;
// Informs the native controller if web usage is allowed or not.
- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled;
// Evaluates the supplied JavaScript in the web view. Calls |handler| with
// results of the evaluation (which may be nil if the implementing object has no
// way to run the evaluation or the evaluation returns a nil value) or an
// NSError if there is an error. The |handler| can be nil.
- (void)evaluateJavaScript:(NSString*)script
         JSONResultHandler:(void (^)(scoped_ptr<base::Value>, NSError*))handler;
// Generates the JavaScript string used to update the UIWebView's URL so that it
// matches the URL displayed in the omnibox and sets window.history.state to
// stateObject. Needed for history.pushState() and history.replaceState().
- (NSString*)javascriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject;
// Injects JavaScript into the web view to update the URL to |URL|, to set
// window.history.state to |stateObject|, and to trigger a popstate() event.
// Upon the scripts completion, resets |urlOnStartLoading_| and
// |_lastRegisteredRequestURL| to |URL|.  This is necessary so that sites that
// depend on URL params/fragments continue to work correctly and that checks for
// the URL don't incorrectly trigger |-pageChanged| calls.
- (void)setPushedOrReplacedURL:(const GURL&)URL
                   stateObject:(NSString*)stateObject;
- (BOOL)isLoaded;
// Called by NSNotificationCenter upon orientation changes.
- (void)orientationDidChange;
// Queries the web view for the user-scalable meta tag and calls
// |-applyPageDisplayState:userScalable:| with the result.
- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState;
// Restores state of the web view's scroll view from |scrollState|.
// |isUserScalable| represents the value of user-scalable meta tag.
- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState
                 userScalable:(BOOL)isUserScalable;
// Calls the zoom-preparation UIScrollViewDelegate callbacks on the web view.
// This is called before |-applyWebViewScrollZoomScaleFromScrollState:|.
- (void)prepareToApplyWebViewScrollZoomScale;
// Calls the zoom-completion UIScrollViewDelegate callbacks on the web view.
// This is called after |-applyWebViewScrollZoomScaleFromScrollState:|.
- (void)finishApplyingWebViewScrollZoomScale;
// Sets scroll offset value for webview scroll view from |scrollState|.
- (void)applyWebViewScrollOffsetFromScrollState:
    (const web::PageScrollState&)scrollState;
// Asynchronously determines whether option |user-scalable| is on in the
// viewport meta of the current web page.
- (void)queryUserScalableProperty:(void (^)(BOOL))responseHandler;
// Asynchronously fetches full width of the rendered web page.
- (void)fetchWebPageWidthWithCompletionHandler:(void (^)(CGFloat))handler;
// Asynchronously fetches information about DOM element for the given point (in
// UIView coordinates). |handler| can not be nil. See |_DOMElementForLastTouch|
// for element format description.
- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:
                 (void (^)(scoped_ptr<base::DictionaryValue>))handler;
// Extracts context menu information from the given DOM element.
// result keys are defined in crw_context_menu_provider.h.
- (NSDictionary*)contextMenuInfoForElement:(base::DictionaryValue*)element;
// Sets the value of |_DOMElementForLastTouch|.
- (void)setDOMElementForLastTouch:(scoped_ptr<base::DictionaryValue>)element;
// Called when the window has determined there was a long-press and context menu
// must be shown.
- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer;
// YES if delegate supports showing context menu by responding to
// webController:runContextMenu:atPoint:inView: selector.
- (BOOL)supportsCustomContextMenu;
// Returns the referrer for the current page.
- (web::Referrer)currentReferrer;
// Presents an error to the user because the CRWWebController cannot verify the
// URL of the current page.
- (void)presentSpoofingError;
// Adds a new CRWSessionEntry with the given URL and state object to the history
// stack. A state object is a serialized generic JavaScript object that contains
// details of the UI's state for a given CRWSessionEntry/URL.
// TODO(stuartmorgan): Move the pushState/replaceState logic into
// NavigationManager.
- (void)pushStateWithPageURL:(const GURL&)pageURL
                 stateObject:(NSString*)stateObject
                  transition:(ui::PageTransition)transition;
// Assigns the given URL and state object to the current CRWSessionEntry.
- (void)replaceStateWithPageURL:(const GURL&)pageUrl
                    stateObject:(NSString*)stateObject;

// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)optOutScrollsToTopForSubviews;
// Tears down the old native controller, and then replaces it with the new one.
- (void)setNativeController:(id<CRWNativeContent>)nativeController;
// Returns whether |url| should be opened.
- (BOOL)shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked;
// Called when |url| needs to be opened in a matching native app.
// Returns YES if the url was succesfully opened in the native app.
- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)url
                         sourceURL:(const GURL&)sourceURL;
// Best guess as to whether the request is a main frame request. This method
// should not be assumed correct for security evaluations, as it is possible to
// spoof.
- (BOOL)isPutativeMainFrameRequest:(NSURLRequest*)request
                       targetFrame:(const web::FrameInfo*)targetFrame;
// Returns whether external URL request should be opened.
- (BOOL)shouldOpenExternalURLRequest:(NSURLRequest*)request
                         targetFrame:(const web::FrameInfo*)targetFrame;
// Called when a page updates its history stack using pushState or replaceState.
- (void)didUpdateHistoryStateWithPageURL:(const GURL&)url;

// Handlers for JavaScript messages. |message| contains a JavaScript command and
// data relevant to the message, and |context| contains contextual information
// about web view state needed for some handlers.

// Handles 'addPluginPlaceholders' message.
- (BOOL)handleAddPluginPlaceholdersMessage:(base::DictionaryValue*)message
                                   context:(NSDictionary*)context;
// Handles 'chrome.send' message.
- (BOOL)handleChromeSendMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context;
// Handles 'console' message.
- (BOOL)handleConsoleMessage:(base::DictionaryValue*)message
                     context:(NSDictionary*)context;
// Handles 'dialog.suppressed' message.
- (BOOL)handleDialogSuppressedMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context;
// Handles 'dialog.willShow' message.
- (BOOL)handleDialogWillShowMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context;
// Handles 'document.favicons' message.
- (BOOL)handleDocumentFaviconsMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context;
// Handles 'document.submit' message.
- (BOOL)handleDocumentSubmitMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context;
// Handles 'externalRequest' message.
- (BOOL)handleExternalRequestMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context;
// Handles 'form.activity' message.
- (BOOL)handleFormActivityMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context;
// Handles 'navigator.credentials.request' message.
- (BOOL)handleCredentialsRequestedMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context;
// Handles 'navigator.credentials.notifySignedIn' message.
- (BOOL)handleSignedInMessage:(base::DictionaryValue*)message
                      context:(NSDictionary*)context;
// Handles 'navigator.credentials.notifySignedOut' message.
- (BOOL)handleSignedOutMessage:(base::DictionaryValue*)message
                       context:(NSDictionary*)context;
// Handles 'navigator.credentials.notifyFailedSignIn' message.
- (BOOL)handleSignInFailedMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context;
// Handles 'resetExternalRequest' message.
- (BOOL)handleResetExternalRequestMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context;
// Handles 'window.close.self' message.
- (BOOL)handleWindowCloseSelfMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context;
// Handles 'window.error' message.
- (BOOL)handleWindowErrorMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context;
// Handles 'window.hashchange' message.
- (BOOL)handleWindowHashChangeMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context;
// Handles 'window.history.back' message.
- (BOOL)handleWindowHistoryBackMessage:(base::DictionaryValue*)message
                               context:(NSDictionary*)context;
// Handles 'window.history.forward' message.
- (BOOL)handleWindowHistoryForwardMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context;
// Handles 'window.history.go' message.
- (BOOL)handleWindowHistoryGoMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context;
@end

namespace {

NSString* const kReferrerHeaderName = @"Referer";  // [sic]

// Full screen experimental setting.

// The long press detection duration must be shorter than the UIWebView's
// long click gesture recognizer's minimum duration. That is 0.55s.
// If our detection duration is shorter, our gesture recognizer will fire
// first, and if it fails the long click gesture (processed simultaneously)
// still is able to complete.
const NSTimeInterval kLongPressDurationSeconds = 0.55 - 0.1;
const CGFloat kLongPressMoveDeltaPixels = 10.0;

// The duration of the period following a screen touch during which the user is
// still considered to be interacting with the page.
const NSTimeInterval kMaximumDelayForUserInteractionInSeconds = 2;

// URLs that are fed into UIWebView as history push/replace get escaped,
// potentially changing their format. Code that attempts to determine whether a
// URL hasn't changed can be confused by those differences though, so method
// will round-trip a URL through the escaping process so that it can be adjusted
// pre-storing, to allow later comparisons to work as expected.
GURL URLEscapedForHistory(const GURL& url) {
  // TODO(stuartmorgan): This is a very large hammer; see if limited unicode
  // escaping would be sufficient.
  return net::GURLWithNSURL(net::NSURLWithGURL(url));
}

// Parses a viewport tag content and returns the value of an attribute with
// the given |name|, or nil if the attribute is not present in the tag.
NSString* GetAttributeValueFromViewPortContent(NSString* attributeName,
                                               NSString* viewPortContent) {
  NSArray* contentItems = [viewPortContent componentsSeparatedByString:@","];
  for (NSString* item in contentItems) {
    NSArray* components = [item componentsSeparatedByString:@"="];
    if ([components count] == 2) {
      NSCharacterSet* spaceAndNewline =
          [NSCharacterSet whitespaceAndNewlineCharacterSet];
      NSString* currentAttributeName =
          [components[0] stringByTrimmingCharactersInSet:spaceAndNewline];
      if ([currentAttributeName isEqualToString:attributeName]) {
        return [components[1] stringByTrimmingCharactersInSet:spaceAndNewline];
      }
    }
  }
  return nil;
}

// Parses a viewport tag content and returns the value of the user-scalable
// attribute or nil.
BOOL GetUserScalablePropertyFromViewPortContent(NSString* viewPortContent) {
  NSString* value =
      GetAttributeValueFromViewPortContent(@"user-scalable", viewPortContent);
  if (!value) {
    return YES;
  }
  return !([value isEqualToString:@"0"] ||
           [value caseInsensitiveCompare:@"no"] == NSOrderedSame);
}

// Leave snapshot overlay up unless page loads.
const NSTimeInterval kSnapshotOverlayDelay = 1.5;
// Transition to fade snapshot overlay.
const NSTimeInterval kSnapshotOverlayTransition = 0.5;

}  // namespace

@implementation CRWWebController

@synthesize webUsageEnabled = _webUsageEnabled;
@synthesize usePlaceholderOverlay = _usePlaceholderOverlay;
@synthesize loadPhase = _loadPhase;

// Implemented by subclasses.
@dynamic keyboardDisplayRequiresUserAction;

+ (instancetype)allocWithZone:(struct _NSZone*)zone {
  if (self == [CRWWebController class]) {
    // This is an abstract class which should not be instantiated directly.
    // Callers should create concrete subclasses instead.
    NOTREACHED();
    return nil;
  }
  return [super allocWithZone:zone];
}

- (instancetype)initWithWebState:(scoped_ptr<WebStateImpl>)webState {
  self = [super init];
  if (self) {
    _webStateImpl = std::move(webState);
    DCHECK(_webStateImpl);
    _webStateImpl->SetWebController(self);
    _webStateImpl->InitializeRequestTracker(self);
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _loadPhase = web::PAGE_LOADED;
    // Content area is lazily instantiated.
    _defaultURL = GURL(url::kAboutBlankURL);
    _jsInjectionReceiver.reset(
        [[CRWJSInjectionReceiver alloc] initWithEvaluator:self]);
    _earlyScriptManager.reset([(CRWJSEarlyScriptManager*)[_jsInjectionReceiver
        instanceOfClass:[CRWJSEarlyScriptManager class]] retain]);
    _windowIDJSManager.reset([(CRWJSWindowIdManager*)[_jsInjectionReceiver
        instanceOfClass:[CRWJSWindowIdManager class]] retain]);
    _lastSeenWindowID.reset();
    _webViewProxy.reset(
        [[CRWWebViewProxyImpl alloc] initWithWebController:self]);
    [[_webViewProxy scrollViewProxy] addObserver:self];
    _gestureRecognizers.reset([[NSMutableArray alloc] init]);
    _webViewToolbars.reset([[NSMutableArray alloc] init]);
    _pendingLoadCompleteActions.reset([[NSMutableArray alloc] init]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(orientationDidChange)
               name:UIApplicationDidChangeStatusBarOrientationNotification
             object:nil];
  }
  return self;
}

- (id<CRWNativeContentProvider>)nativeProvider {
  return _nativeProvider.get();
}

- (void)setNativeProvider:(id<CRWNativeContentProvider>)nativeProvider {
  _nativeProvider.reset(nativeProvider);
}

- (id<CRWSwipeRecognizerProvider>)swipeRecognizerProvider {
  return _swipeRecognizerProvider.get();
}

- (void)setSwipeRecognizerProvider:
    (id<CRWSwipeRecognizerProvider>)swipeRecognizerProvider {
  _swipeRecognizerProvider.reset(swipeRecognizerProvider);
}

- (WebState*)webState {
  return _webStateImpl.get();
}

- (WebStateImpl*)webStateImpl {
  return _webStateImpl.get();
}

- (void)clearTransientContentView {
  // Early return if there is no transient content view.
  if (!self.containerView.transientContentView)
    return;

  // Remove the transient content view from the hierarchy.
  [self.containerView clearTransientContentView];

  // Notify the WebState so it can perform any required state cleanup.
  if (_webStateImpl)
    _webStateImpl->ClearTransientContentView();
}

- (void)showTransientContentView:(CRWContentView*)contentView {
  DCHECK(contentView);
  DCHECK(contentView.scrollView);
  // TODO(crbug.com/556848) Reenable DCHECK when |CRWWebControllerContainerView|
  // is restructured so that subviews are not added during |layoutSubviews|.
  // DCHECK([contentView.scrollView isDescendantOfView:contentView]);
  [self.containerView displayTransientContent:contentView];
}

- (id<CRWWebDelegate>)delegate {
  return _delegate.get();
}

- (void)setDelegate:(id<CRWWebDelegate>)delegate {
  _delegate.reset(delegate);
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)]) {
    if ([_delegate respondsToSelector:@selector(webController:titleDidChange:)])
      [self.nativeController setDelegate:self];
    else
      [self.nativeController setDelegate:nil];
  }
}

- (id<CRWWebUserInterfaceDelegate>)UIDelegate {
  return _UIDelegate.get();
}

- (void)setUIDelegate:(id<CRWWebUserInterfaceDelegate>)UIDelegate {
  _UIDelegate.reset(UIDelegate);
}

- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script {
  NSString* kTemplate = @"if (__gCrWeb['windowId'] === '%@') { %@; }";
  return [NSString stringWithFormat:kTemplate, [self windowId], script];
}

- (void)removeWebViewAllowingCachedReconstruction:(BOOL)allowCache {
  if (!self.webView)
    return;

  SEL cancelDialogsSelector =
      @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];

  if (allowCache)
    _expectedReconstructionURL = [self currentNavigationURL];
  else
    _expectedReconstructionURL = GURL();

  [self abortLoad];
  [self.webView removeFromSuperview];
  [self.containerView resetContent];
  [self resetWebView];
}

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
  DCHECK(_isBeingDestroyed);  // 'close' must have been called already.
  DCHECK(!self.webView);
  _touchTrackingRecognizer.get().touchTrackingDelegate = nil;
  [[_webViewProxy scrollViewProxy] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (BOOL)runUnloadListenerBeforeClosing {
  // There's not much that can be done since there's limited access to WebKit.
  // Always return that it's ok to close immediately.
  return YES;
}

- (void)dismissKeyboard {
  [self.webView endEditing:YES];
  if ([self.nativeController respondsToSelector:@selector(dismissKeyboard)])
    [self.nativeController dismissKeyboard];
}

- (id<CRWNativeContent>)nativeController {
  return self.containerView.nativeController;
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  // Check for pointer equality.
  if (self.nativeController == nativeController)
    return;

  // Unset the delegate on the previous instance.
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)])
    [self.nativeController setDelegate:nil];

  [self.containerView displayNativeContent:nativeController];
  [self setNativeControllerWebUsageEnabled:_webUsageEnabled];
}

// NativeControllerDelegate method, called to inform that title has changed.
- (void)nativeContent:(id)content titleDidChange:(NSString*)title {
  // Responsiveness to delegate method was checked in setDelegate:.
  [_delegate webController:self titleDidChange:title];
}

- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled {
  if ([self.nativeController
          respondsToSelector:@selector(setWebUsageEnabled:)]) {
    [self.nativeController setWebUsageEnabled:webUsageEnabled];
  }
}

- (void)setWebUsageEnabled:(BOOL)enabled {
  if (_webUsageEnabled == enabled)
    return;
  _webUsageEnabled = enabled;

  // WKWebView autoreleases its WKProcessPool on removal from superview.
  // Deferring WKProcessPool deallocation may lead to issues with cookie
  // clearing and and Browsing Data Partitioning implementation.
  @autoreleasepool {
    [self setNativeControllerWebUsageEnabled:_webUsageEnabled];
    if (enabled) {
      // Don't create the web view; let it be lazy created as needed.
    } else {
      [self clearTransientContentView];
      [self removeWebViewAllowingCachedReconstruction:YES];
      _touchTrackingRecognizer.get().touchTrackingDelegate = nil;
      _touchTrackingRecognizer.reset();
      [self resetContainerView];
    }
  }
}

- (void)requirePageReconstruction {
  [self removeWebViewAllowingCachedReconstruction:NO];
}

- (void)requirePageReload {
  _requireReloadOnDisplay = YES;
}

- (void)resetContainerView {
  [self.containerView removeFromSuperview];
  _containerView.reset();
}

- (void)handleLowMemory {
  [self removeWebViewAllowingCachedReconstruction:YES];
  _touchTrackingRecognizer.get().touchTrackingDelegate = nil;
  _touchTrackingRecognizer.reset();
  [self resetContainerView];
  _usePlaceholderOverlay = YES;
}

- (void)reinitializeWebViewAndReload:(BOOL)reload {
  if (self.webView) {
    [self removeWebViewAllowingCachedReconstruction:NO];
    if (reload) {
      [self loadCurrentURLInWebView];
    } else {
      // Clear the space for the web view to lazy load when needed.
      _usePlaceholderOverlay = YES;
      _touchTrackingRecognizer.get().touchTrackingDelegate = nil;
      _touchTrackingRecognizer.reset();
      [self resetContainerView];
    }
  }
}

- (void)childWindowClosed:(NSString*)windowName {
  // Subclasses can override this method to be informed about a closed window.
}

- (BOOL)isViewAlive {
  return [self.containerView isViewAlive];
}

- (BOOL)contentIsHTML {
  return [self webViewDocumentType] == web::WEB_VIEW_DOCUMENT_TYPE_HTML;
}

// Stop doing stuff, especially network stuff. Close the request tracker.
- (void)terminateNetworkActivity {
  DCHECK(!_isHalted);
  _isHalted = YES;

  // Cancel all outstanding perform requests, and clear anything already queued
  // (since this may be called from within the handling loop) to prevent any
  // asynchronous JavaScript invocation handling from continuing.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  _webStateImpl->CloseRequestTracker();
}

- (void)dismissModals {
  if ([self.nativeController respondsToSelector:@selector(dismissModals)])
    [self.nativeController dismissModals];
}

// Caller must reset the delegate before calling.
- (void)close {
  self.nativeProvider = nil;
  self.swipeRecognizerProvider = nil;
  if ([self.nativeController respondsToSelector:@selector(close)])
    [self.nativeController close];

  base::scoped_nsobject<NSSet> observers([_observers copy]);
  for (id it in observers.get()) {
    if ([it respondsToSelector:@selector(webControllerWillClose:)])
      [it webControllerWillClose:self];
  }

  if (!_isHalted) {
    [self terminateNetworkActivity];
  }

  DCHECK(!_isBeingDestroyed);
  DCHECK(!_delegate);  // Delegate should reset its association before closing.
  // Mark the destruction sequence has started, in case someone else holds a
  // strong reference and tries to continue using the tab.
  _isBeingDestroyed = YES;

  // Remove the web view now. Otherwise, delegate callbacks occur.
  [self removeWebViewAllowingCachedReconstruction:NO];

  // Tear down web ui (in case this is part of this tab) and web state now,
  // since the timing of dealloc can't be guaranteed.
  _webStateImpl.reset();
}

- (void)checkLinkPresenceUnderGesture:(UIGestureRecognizer*)gestureRecognizer
                    completionHandler:(void (^)(BOOL))completionHandler {
  CGPoint webViewPoint = [gestureRecognizer locationInView:self.webView];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self fetchDOMElementAtPoint:webViewPoint
             completionHandler:^(scoped_ptr<base::DictionaryValue> element) {
               std::string link;
               BOOL hasLink =
                   element && element->GetString("href", &link) && link.size();
               completionHandler(hasLink);
             }];
}

- (void)setDOMElementForLastTouch:(scoped_ptr<base::DictionaryValue>)element {
  _DOMElementForLastTouch = std::move(element);
}

- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer {
  // Calling this method if [self supportsCustomContextMenu] returned NO
  // is a programmer error.
  DCHECK([self supportsCustomContextMenu]);

  // We don't want ongoing notification that the long press is held.
  if ([gestureRecognizer state] != UIGestureRecognizerStateBegan)
    return;

  if (!_DOMElementForLastTouch || _DOMElementForLastTouch->empty())
    return;

  NSDictionary* info =
      [self contextMenuInfoForElement:_DOMElementForLastTouch.get()];
  CGPoint point = [gestureRecognizer locationInView:self.webView];

  // Cancel all touches on the web view when showing custom context menu. This
  // will suppress the system context menu and prevent further user interactions
  // with web view (like scrolling the content and following links). This
  // approach is similar to UIWebView and WKWebView workflow as both call
  // -[UIApplication _cancelAllTouches] to cancel all touch events, once the
  // long press is detected.
  CancelAllTouches(self.webScrollView);
  [self.UIDelegate webController:self
                  runContextMenu:info
                         atPoint:point
                          inView:self.webView];
}

- (BOOL)supportsCustomContextMenu {
  SEL runMenuSelector = @selector(webController:runContextMenu:atPoint:inView:);
  return [self.UIDelegate respondsToSelector:runMenuSelector];
}

// TODO(shreyasv): This code is shared with SnapshotManager. Remove this and add
// it as part of WebDelegate delegate API such that a default image is returned
// immediately.
+ (UIImage*)defaultSnapshotImage {
  static UIImage* defaultImage = nil;

  if (!defaultImage) {
    CGRect frame = CGRectMake(0, 0, 2, 2);
    UIGraphicsBeginImageContext(frame.size);
    [[UIColor whiteColor] setFill];
    CGContextFillRect(UIGraphicsGetCurrentContext(), frame);

    UIImage* result = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    defaultImage =
        [[result stretchableImageWithLeftCapWidth:1 topCapHeight:1] retain];
  }
  return defaultImage;
}

- (BOOL)canGoBack {
  return _webStateImpl->GetNavigationManagerImpl().CanGoBack();
}

- (BOOL)canGoForward {
  return _webStateImpl->GetNavigationManagerImpl().CanGoForward();
}

- (CGPoint)scrollPosition {
  CGPoint position = CGPointMake(0.0, 0.0);
  if (!self.webScrollView)
    return position;
  return self.webScrollView.contentOffset;
}

- (BOOL)atTop {
  if (!self.webView)
    return YES;
  UIScrollView* scrollView = self.webScrollView;
  return scrollView.contentOffset.y == -scrollView.contentInset.top;
}

- (void)presentSpoofingError {
  UMA_HISTOGRAM_ENUMERATION("Web.URLVerificationFailure",
                            [self webViewDocumentType],
                            web::WEB_VIEW_DOCUMENT_TYPE_COUNT);
  if (self.webView) {
    [self removeWebViewAllowingCachedReconstruction:NO];
    [_delegate presentSpoofingError];
  }
}

- (GURL)currentURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel) << "Verification of the trustLevel state is mandatory";
  if (self.webView) {
    GURL url([self webURLWithTrustLevel:trustLevel]);
    // Web views treat all about: URLs as the same origin, which makes it
    // possible for pages to document.write into about:<foo> pages, where <foo>
    // can be something misleading. Report any about: URL as about:blank to
    // prevent that. See crbug.com/326118
    if (url.scheme() == url::kAboutScheme)
      return GURL(url::kAboutBlankURL);
    return url;
  }
  // Any non-web URL source is trusted.
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  if (self.nativeController)
    return [self.nativeController url];
  return [self currentNavigationURL];
}

- (GURL)currentURL {
  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  const GURL url([self currentURLWithTrustLevel:&trustLevel]);

  // Check whether the spoofing warning needs to be displayed.
  if (trustLevel == web::URLVerificationTrustLevel::kNone &&
      ![self ignoreURLVerificationFailures]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!_isHalted) {
        DCHECK_EQ(url, [self currentNavigationURL]);
        [self presentSpoofingError];
      }
    });
  }

  return url;
}

- (web::Referrer)currentReferrer {
  // Referrer string doesn't include the fragment, so in cases where the
  // previous URL is equal to the current referrer plus the fragment the
  // previous URL is returned as current referrer.
  NSString* referrerString = self.currentReferrerString;

  // In case of an error evaluating the JavaScript simply return empty string.
  if ([referrerString length] == 0)
    return web::Referrer();

  NSString* previousURLString =
      base::SysUTF8ToNSString([self currentNavigationURL].spec());
  // Check if the referrer is equal to the previous URL minus the hash symbol.
  // L'#' is used to convert the char '#' to a unichar.
  if ([previousURLString length] > [referrerString length] &&
      [previousURLString hasPrefix:referrerString] &&
      [previousURLString characterAtIndex:[referrerString length]] == L'#') {
    referrerString = previousURLString;
  }
  // Since referrer is being extracted from the destination page, the correct
  // policy from the origin has *already* been applied. Since the extracted URL
  // is the post-policy value, and the source policy is no longer available,
  // the policy is set to Always so that whatever WebKit decided to send will be
  // re-sent when replaying the entry.
  // TODO(stuartmorgan): When possible, get the real referrer and policy in
  // advance and use that instead. https://crbug.com/227769.
  return web::Referrer(GURL(base::SysNSStringToUTF8(referrerString)),
                       web::ReferrerPolicyAlways);
}

- (void)pushStateWithPageURL:(const GURL&)pageURL
                 stateObject:(NSString*)stateObject
                  transition:(ui::PageTransition)transition {
  [[self sessionController] pushNewEntryWithURL:pageURL
                                    stateObject:stateObject
                                     transition:transition];
  [self didUpdateHistoryStateWithPageURL:pageURL];
  self.userInteractionRegistered = NO;
}

- (void)replaceStateWithPageURL:(const GURL&)pageUrl
                    stateObject:(NSString*)stateObject {
  [[self sessionController] updateCurrentEntryWithURL:pageUrl
                                          stateObject:stateObject];
  [self didUpdateHistoryStateWithPageURL:pageUrl];
}

- (void)injectEarlyInjectionScripts {
  DCHECK(self.webView);
  if (![_earlyScriptManager hasBeenInjected]) {
    [_earlyScriptManager inject];
    // If this assertion fires there has been an error parsing the core.js
    // object.
    DCHECK([_earlyScriptManager hasBeenInjected]);
  }
  [self injectWindowID];
}

- (void)injectWindowID {
  if (![_windowIDJSManager hasBeenInjected]) {
    // If the window ID wasn't present, this is a new page.
    [self setPageChangeProbability:web::PAGE_CHANGE_PROBABILITY_LOW];
    // Default values for suppressDialogs and notifyAboutDialogs are NO,
    // so updating them only when necessary is a good optimization.
    if (_setSuppressDialogsLater || _setNotifyAboutDialogsLater) {
      [self setSuppressDialogs:_setSuppressDialogsLater
                        notify:_setNotifyAboutDialogsLater];
      _setSuppressDialogsLater = NO;
      _setNotifyAboutDialogsLater = NO;
    }

    [_windowIDJSManager inject];
    DCHECK([_windowIDJSManager hasBeenInjected]);
  }
}

// Set the specified recognizer to take priority over any recognizers in the
// view that have a description containing the specified text fragment.
+ (void)requireGestureRecognizerToFail:(UIGestureRecognizer*)recognizer
                                inView:(UIView*)view
                 containingDescription:(NSString*)fragment {
  for (UIGestureRecognizer* iRecognizer in [view gestureRecognizers]) {
    if (iRecognizer != recognizer) {
      NSString* description = [iRecognizer description];
      if ([description rangeOfString:fragment].location != NSNotFound) {
        [iRecognizer requireGestureRecognizerToFail:recognizer];
        // requireGestureRecognizerToFail: doesn't retain the recognizer, so it
        // is possible for |iRecognizer| to outlive |recognizer| and end up with
        // a dangling pointer. Add a retaining associative reference to ensure
        // that the lifetimes work out.
        // Note that normally using the value as the key wouldn't make any
        // sense, but here it's fine since nothing needs to look up the value.
        objc_setAssociatedObject(view, recognizer, recognizer,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
    }
  }
}

- (void)webViewDidChange {
  CHECK(_webUsageEnabled) << "Tried to create a web view while suspended!";

  UIView* webView = self.webView;
  DCHECK(webView);

  [webView setTag:kWebViewTag];
  [webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                               UIViewAutoresizingFlexibleHeight];
  [webView setBackgroundColor:[UIColor colorWithWhite:0.2 alpha:1.0]];

  // Create a dependency between the |webView| pan gesture and BVC side swipe
  // gestures. Note: This needs to be added before the longPress recognizers
  // below, or the longPress appears to deadlock the remaining recognizers,
  // thereby breaking scroll.
  NSSet* recognizers = [_swipeRecognizerProvider swipeRecognizers];
  for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
    [self.webScrollView.panGestureRecognizer
        requireGestureRecognizerToFail:swipeRecognizer];
  }

  // On iOS 4.x, there are two gesture recognizers on the UIWebView subclasses,
  // that have a minimum tap threshold of 0.12s and 0.75s.
  //
  // My theory is that the shorter threshold recognizer performs the link
  // highlight (grey highlight around links when it is tapped and held) while
  // the longer threshold one pops up the context menu.
  //
  // To override the context menu, this recognizer needs to react faster than
  // the 0.75s one. The below gesture recognizer is initialized with a
  // detection duration a little lower than that (see
  // kLongPressDurationSeconds). It also points the delegate to this class that
  // allows simultaneously operate along with the other recognizers.
  _contextMenuRecognizer.reset([[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(showContextMenu:)]);
  [_contextMenuRecognizer setMinimumPressDuration:kLongPressDurationSeconds];
  [_contextMenuRecognizer setAllowableMovement:kLongPressMoveDeltaPixels];
  [_contextMenuRecognizer setDelegate:self];
  [webView addGestureRecognizer:_contextMenuRecognizer];
  // Certain system gesture handlers are known to conflict with our context
  // menu handler, causing extra events to fire when the context menu is active.

  // A number of solutions have been investigated. The lowest-risk solution
  // appears to be to recurse through the web controller's recognizers, looking
  // for fingerprints of the recognizers known to cause problems, which are then
  // de-prioritized (below our own long click handler).
  // Hunting for description fragments of system recognizers is undeniably
  // brittle for future versions of iOS. If it does break the context menu
  // events may leak (regressing b/5310177), but the app will otherwise work.
  [CRWWebController
      requireGestureRecognizerToFail:_contextMenuRecognizer
                              inView:webView
               containingDescription:@"action=_highlightLongPressRecognized:"];

  // Add all additional gesture recognizers to the web view.
  for (UIGestureRecognizer* recognizer in _gestureRecognizers.get()) {
    [webView addGestureRecognizer:recognizer];
  }

  _URLOnStartLoading = _defaultURL;

  // Add the web toolbars.
  [self.containerView addToolbars:_webViewToolbars];

  base::scoped_nsobject<CRWWebViewContentView> webViewContentView(
      [[CRWWebViewContentView alloc] initWithWebView:self.webView
                                          scrollView:self.webScrollView]);
  [self.containerView displayWebViewContentView:webViewContentView];
}

- (CRWWebController*)createChildWebController {
  CRWWebController* result = [self.delegate webPageOrderedOpen];
  DCHECK(!result || result.sessionController.openedByDOM);
  return result;
}

- (BOOL)canUseViewForGeneratingOverlayPlaceholderView {
  return self.containerView != nil;
}

- (UIView*)view {
  // Kick off the process of lazily creating the view and starting the load if
  // necessary; this creates _containerView if it doesn't exist.
  [self triggerPendingLoad];
  DCHECK(self.containerView);
  return self.containerView;
}

- (CRWWebControllerContainerView*)containerView {
  return _containerView.get();
}

- (id<CRWWebViewProxy>)webViewProxy {
  return _webViewProxy.get();
}

- (UIView*)viewForPrinting {
  // TODO(ios): crbug.com/227944. Printing is not supported for native
  // controllers.
  return self.webView;
}

- (void)loadRequest:(NSMutableURLRequest*)request {
  // Subclasses must implement this method.
  NOTREACHED();
}

- (void)registerLoadRequest:(const GURL&)requestURL
                   referrer:(const web::Referrer&)referrer
                 transition:(ui::PageTransition)transition {
  // Transfer time is registered so that further transitions within the time
  // envelope are not also registered as links.
  _lastTransferTimeInSeconds = CFAbsoluteTimeGetCurrent();
  // Before changing phases, the delegate should be informed that any existing
  // request is being cancelled before completion.
  [self loadCancelled];
  DCHECK(_loadPhase == web::PAGE_LOADED);

  _loadPhase = web::LOAD_REQUESTED;
  [self resetLoadState];
  _lastRegisteredRequestURL = requestURL;

  if (!(transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK)) {
    // Record state of outgoing page.
    [self recordStateInHistory];
  }

  // If the web view had been discarded, and this request is to load that
  // URL again, then it's a rebuild and should use the cache.
  BOOL preferCache = _expectedReconstructionURL.is_valid() &&
                     _expectedReconstructionURL == requestURL;

  [_delegate webWillAddPendingURL:requestURL transition:transition];
  // Add or update pending url.
  if (_webStateImpl->GetNavigationManagerImpl().GetPendingItem()) {
    // Update the existing pending entry.
    // Typically on PAGE_TRANSITION_CLIENT_REDIRECT.
    [[self sessionController] updatePendingEntry:requestURL];
  } else {
    // A new session history entry needs to be created.
    [[self sessionController] addPendingEntry:requestURL
                                     referrer:referrer
                                   transition:transition
                            rendererInitiated:YES];
  }
  // Update the cache mode for all the network requests issued by this web view.
  // The mode is reset to CACHE_NORMAL after each page load.
  if (_webStateImpl->GetCacheMode() != net::RequestTracker::CACHE_NORMAL) {
    _webStateImpl->GetRequestTracker()->SetCacheModeFromUIThread(
        _webStateImpl->GetCacheMode());
  } else if (preferCache) {
    _webStateImpl->GetRequestTracker()->SetCacheModeFromUIThread(
        net::RequestTracker::CACHE_HISTORY);
  }
  _webStateImpl->SetIsLoading(true);
  [_delegate webDidAddPendingURL];
  _webStateImpl->OnProvisionalNavigationStarted(requestURL);
}

- (NSString*)javascriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject {
  std::string outURL;
  base::EscapeJSONString(URL.spec(), true, &outURL);
  return
      [NSString stringWithFormat:@"__gCrWeb.replaceWebViewURL(%@, %@);",
                                 base::SysUTF8ToNSString(outURL), stateObject];
}

- (void)setPushedOrReplacedURL:(const GURL&)URL
                   stateObject:(NSString*)stateObject {
  // TODO(stuartmorgan): Make CRWSessionController manage this internally (or
  // remove it; it's not clear this matches other platforms' behavior).
  _webStateImpl->GetNavigationManagerImpl().OnNavigationItemCommitted();

  NSString* replaceWebViewUrlJS =
      [self javascriptToReplaceWebViewURL:URL stateObjectJSON:stateObject];
  std::string outState;
  base::EscapeJSONString(base::SysNSStringToUTF8(stateObject), true, &outState);
  NSString* popstateJS =
      [NSString stringWithFormat:@"__gCrWeb.dispatchPopstateEvent(%@);",
                                 base::SysUTF8ToNSString(outState)];
  NSString* combinedJS =
      [NSString stringWithFormat:@"%@%@", replaceWebViewUrlJS, popstateJS];
  GURL urlCopy(URL);
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self evaluateJavaScript:combinedJS
       stringResultHandler:^(NSString*, NSError*) {
         if (!weakSelf || weakSelf.get()->_isBeingDestroyed)
           return;
         base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
         strongSelf.get()->_URLOnStartLoading = urlCopy;
         strongSelf.get()->_lastRegisteredRequestURL = urlCopy;
       }];
}

// Load the current URL in a web view, first ensuring the web view is visible.
- (void)loadCurrentURLInWebView {
  [self willLoadCurrentURLInWebView];

  // Clear the set of URLs opened in external applications.
  _openedApplicationURL.reset([[NSMutableSet alloc] init]);

  // Load the url. The UIWebView delegate callbacks take care of updating the
  // session history and UI.
  const GURL targetURL([self currentNavigationURL]);
  if (!targetURL.is_valid())
    return;

  // JavaScript should never be evaluated here. User-entered JS should be
  // evaluated via stringByEvaluatingUserJavaScriptFromString.
  DCHECK(!targetURL.SchemeIs(url::kJavaScriptScheme));

  [self ensureWebViewCreated];

  [self loadRequestForCurrentNavigationItem];
}

- (void)loadRequestForCurrentNavigationItem {
  // Handled differently by UIWebView and WKWebView subclasses.
  NOTIMPLEMENTED();
}

- (NSMutableURLRequest*)requestForCurrentNavigationItem {
  const GURL currentNavigationURL([self currentNavigationURL]);
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:net::NSURLWithGURL(currentNavigationURL)];
  const web::Referrer referrer([self currentSessionEntryReferrer]);
  if (referrer.url.is_valid()) {
    std::string referrerValue =
        web::ReferrerHeaderValueForNavigation(currentNavigationURL, referrer);
    if (!referrerValue.empty()) {
      [request setValue:base::SysUTF8ToNSString(referrerValue)
          forHTTPHeaderField:kReferrerHeaderName];
    }
  }

  // If there are headers in the current session entry add them to |request|.
  // Headers that would overwrite fields already present in |request| are
  // skipped.
  NSDictionary* headers = [self currentHTTPHeaders];
  for (NSString* headerName in headers) {
    if (![request valueForHTTPHeaderField:headerName]) {
      [request setValue:[headers objectForKey:headerName]
          forHTTPHeaderField:headerName];
    }
  }

  return request;
}

- (void)loadNativeViewWithSuccess:(BOOL)loadSuccess {
  const GURL currentURL([self currentURL]);
  [self didStartLoadingURL:currentURL updateHistory:loadSuccess];
  _loadPhase = web::PAGE_LOADED;

  // Perform post-load-finished updates.
  [self didFinishWithURL:currentURL loadSuccess:loadSuccess];

  // Inform the embedder the title changed.
  if ([_delegate respondsToSelector:@selector(webController:titleDidChange:)]) {
    NSString* title = [self.nativeController title];
    // If a title is present, notify the delegate.
    if (title)
      [_delegate webController:self titleDidChange:title];
    // If the controller handles title change notification, route those to the
    // delegate.
    if ([self.nativeController respondsToSelector:@selector(setDelegate:)]) {
      [self.nativeController setDelegate:self];
    }
  }
}

- (void)loadErrorInNativeView:(NSError*)error {
  [self removeWebViewAllowingCachedReconstruction:NO];

  const GURL currentUrl = [self currentNavigationURL];

  error = web::NetErrorFromError(error);
  BOOL isPost = [self isCurrentNavigationItemPOST];
  [self setNativeController:[_nativeProvider controllerForURL:currentUrl
                                                    withError:error
                                                       isPost:isPost]];
  [self loadNativeViewWithSuccess:NO];
}

// Load the current URL in a native controller, retrieved from the native
// provider. Call |loadNativeViewWithSuccess:YES| to load the native controller.
- (void)loadCurrentURLInNativeView {
  // Free the web view.
  [self removeWebViewAllowingCachedReconstruction:NO];

  const GURL targetURL = [self currentNavigationURL];
  const web::Referrer referrer;
  // Unlike the WebView case, always create a new controller and view.
  // TODO(pinkerton): What to do if this does return nil?
  [self setNativeController:[_nativeProvider controllerForURL:targetURL]];
  [self registerLoadRequest:targetURL
                   referrer:referrer
                 transition:[self currentTransition]];
  [self loadNativeViewWithSuccess:YES];
}

- (void)loadWithParams:(const web::WebLoadParams&)originalParams {
  // Make a copy of |params|, as some of the delegate methods may modify it.
  web::WebLoadParams params(originalParams);

  // Initiating a navigation from the UI, record the current page state before
  // the new page loads. Don't record for back/forward, as the current entry
  // has already been moved to the next entry in the history. Do, however,
  // record it for general reload.
  // TODO(jimblackler): consider a single unified call to record state whenever
  // the page is about to be changed. This cannot currently be done after
  // addPendingEntry is called.

  [_delegate webWillInitiateLoadWithParams:params];

  GURL navUrl = params.url;
  ui::PageTransition transition = params.transition_type;

  BOOL initialNavigation = NO;
  BOOL forwardBack =
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) &&
      (transition & ui::PAGE_TRANSITION_FORWARD_BACK);
  if (forwardBack) {
    // Setting these for back/forward is not supported.
    DCHECK(!params.extra_headers);
    DCHECK(!params.post_data);
  } else {
    // Clear transient view before making any changes to history and navigation
    // manager. TODO(stuartmorgan): Drive Transient Item clearing from
    // navigation system, rather than from WebController.
    [self clearTransientContentView];

    // TODO(stuartmorgan): Why doesn't recordStateInHistory get called for
    // forward/back transitions?
    [self recordStateInHistory];

    CRWSessionController* history =
        _webStateImpl->GetNavigationManagerImpl().GetSessionController();
    if (!self.currentSessionEntry)
      initialNavigation = YES;
    [history addPendingEntry:navUrl
                    referrer:params.referrer
                  transition:transition
           rendererInitiated:params.is_renderer_initiated];
    web::NavigationItemImpl* addedItem =
        [self currentSessionEntry].navigationItemImpl;
    DCHECK(addedItem);
    if (params.extra_headers)
      addedItem->AddHttpRequestHeaders(params.extra_headers);
    if (params.post_data) {
      DCHECK([addedItem->GetHttpRequestHeaders() objectForKey:@"Content-Type"])
          << "Post data should have an associated content type";
      addedItem->SetPostData(params.post_data);
      addedItem->SetShouldSkipResubmitDataConfirmation(true);
    }
  }

  [_delegate webDidUpdateSessionForLoadWithParams:params
                             wasInitialNavigation:initialNavigation];

  // If a non-default cache mode is passed in, it takes precedence over
  // |reload|.
  const BOOL reload = [self shouldReload:navUrl transition:transition];
  if (params.cache_mode != net::RequestTracker::CACHE_NORMAL) {
    _webStateImpl->SetCacheMode(params.cache_mode);
  } else if (reload) {
    _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_RELOAD);
  }

  [self loadCurrentURL];

  // Change the cache mode back to CACHE_NORMAL after a reload.
  _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_NORMAL);
}

- (void)loadCurrentURL {
  // If the content view doesn't exist, the tab has either been evicted, or
  // never displayed. Bail, and let the URL be loaded when the tab is shown.
  if (!self.containerView)
    return;

  // Reset current WebUI if one exists.
  [self clearWebUI];

  // Precaution, so that the outgoing URL is registered, to reduce the risk of
  // it being seen as a fresh URL later by the same method (and new page change
  // erroneously reported).
  [self checkForUnexpectedURLChange];

  // Abort any outstanding page load. This ensures the delegate gets informed
  // about the outgoing page, and further messages from the page are suppressed.
  if (_loadPhase != web::PAGE_LOADED)
    [self abortLoad];

  DCHECK(!_isHalted);
  // Remove the transient content view.
  [self clearTransientContentView];

  const GURL currentURL = [self currentNavigationURL];
  // If it's a chrome URL, but not a native one, create the WebUI instance.
  if (web::GetWebClient()->IsAppSpecificURL(currentURL) &&
      ![_nativeProvider hasControllerForURL:currentURL]) {
    [self createWebUIForURL:currentURL];
  }

  // Loading a new url, must check here if it's a native chrome URL and
  // replace the appropriate view if so, or transition back to a web view from
  // a native view.
  if ([self shouldLoadURLInNativeView:currentURL]) {
    [self loadCurrentURLInNativeView];
  } else {
    [self loadCurrentURLInWebView];
  }

  // Once a URL has been loaded, any cached-based reconstruction state has
  // either been handled or obsoleted.
  _expectedReconstructionURL = GURL();
}

- (BOOL)shouldLoadURLInNativeView:(const GURL&)url {
  // App-specific URLs that don't require WebUI are loaded in native views.
  return web::GetWebClient()->IsAppSpecificURL(url) &&
         !_webStateImpl->HasWebUI();
}

- (void)triggerPendingLoad {
  if (!self.containerView) {
    DCHECK(!_isBeingDestroyed);
    // Create the top-level parent view, which will contain the content (whether
    // native or web). Note, this needs to be created with a non-zero size
    // to allow for (native) subviews with autosize constraints to be correctly
    // processed.
    _containerView.reset(
        [[CRWWebControllerContainerView alloc] initWithDelegate:self]);

    // Compute and set the frame of the containerView.
    CGFloat statusBarHeight =
        [[UIApplication sharedApplication] statusBarFrame].size.height;
    CGRect containerViewFrame =
        [UIApplication sharedApplication].keyWindow.bounds;
    containerViewFrame.origin.y += statusBarHeight;
    containerViewFrame.size.height -= statusBarHeight;
    self.containerView.frame = containerViewFrame;
    DCHECK(!CGRectIsEmpty(self.containerView.frame));

    [self.containerView addGestureRecognizer:[self touchTrackingRecognizer]];
    [self.containerView setAccessibilityIdentifier:web::kContainerViewID];
    // Is |currentUrl| a web scheme or native chrome scheme.
    BOOL isChromeScheme =
        web::GetWebClient()->IsAppSpecificURL([self currentNavigationURL]);

    // Don't immediately load the web page if in overlay mode. Always load if
    // native.
    if (isChromeScheme || !_overlayPreviewMode) {
      // TODO(jimblackler): end the practice of calling |loadCurrentURL| when it
      // is possible there is no current URL. If the call performs necessary
      // initialization, break that out.
      [self loadCurrentURL];
    }

    // Display overlay view until current url has finished loading or delay and
    // then transition away.
    if ((_overlayPreviewMode || _usePlaceholderOverlay) && !isChromeScheme)
      [self addPlaceholderOverlay];

    // Don't reset the overlay flag if in preview mode.
    if (!_overlayPreviewMode)
      _usePlaceholderOverlay = NO;
  } else if (_requireReloadOnDisplay && self.webView) {
    _requireReloadOnDisplay = NO;
    [self addPlaceholderOverlay];
    [self loadCurrentURL];
  }
}

- (BOOL)shouldReload:(const GURL&)destinationURL
          transition:(ui::PageTransition)transition {
  // Do a reload if the user hits enter in the address bar or re-types a URL.
  CRWSessionController* sessionController =
      _webStateImpl->GetNavigationManagerImpl().GetSessionController();
  web::NavigationItem* item =
      _webStateImpl->GetNavigationManagerImpl().GetVisibleItem();
  return (transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) && item &&
         (destinationURL == item->GetURL() ||
          destinationURL == [sessionController currentEntry].originalUrl);
}

// Reload either the web view or the native content depending on which is
// displayed.
- (void)reloadInternal {
  // Clear last user interaction.
  // TODO(crbug.com/546337): Move to after the load commits, in the subclass
  // implementation. This will be inaccurate if the reload fails or is
  // cancelled.
  _lastUserInteraction.reset();
  web::RecordAction(UserMetricsAction("Reload"));
  if (self.webView) {
    web::NavigationItem* transientItem =
        _webStateImpl->GetNavigationManagerImpl().GetTransientItem();
    if (transientItem) {
      // If there's a transient item, a reload is considered a new navigation to
      // the transient item's URL (as on other platforms).
      web::WebLoadParams reloadParams(transientItem->GetURL());
      reloadParams.transition_type = ui::PAGE_TRANSITION_RELOAD;
      reloadParams.extra_headers.reset(
          [transientItem->GetHttpRequestHeaders() copy]);
      [self loadWithParams:reloadParams];
    } else {
      // As with back and forward navigation, load the URL manually instead of
      // using the web view's reload. This ensures state processing and delegate
      // calls are consistent.
      // TODO(eugenebut): revisit this for WKWebView.
      [self loadCurrentURL];
    }
  } else {
    [self.nativeController reload];
  }
}

- (void)reload {
  [_delegate webWillReload];

  _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_RELOAD);
  [self reloadInternal];
  _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_NORMAL);
}

- (void)loadCancelled {
  // Current load will not complete; this should be communicated upstream to the
  // delegate.
  switch (_loadPhase) {
    case web::LOAD_REQUESTED:
      // Load phase after abort is always PAGE_LOADED.
      _loadPhase = web::PAGE_LOADED;
      if (!_isHalted) {
        _webStateImpl->SetIsLoading(false);
      }
      [_delegate webCancelStartLoadingRequest];
      break;
    case web::PAGE_LOADING:
      // The previous load never fully completed before this page change. The
      // loadPhase is changed to PAGE_LOADED to indicate the cycle is complete,
      // and the delegate is called.
      _loadPhase = web::PAGE_LOADED;
      if (!_isHalted) {
        // RequestTracker expects StartPageLoad to be followed by
        // FinishPageLoad, passing the exact same URL.
        self.webStateImpl->GetRequestTracker()->FinishPageLoad(
            _URLOnStartLoading, false);
      }
      [_delegate webLoadCancelled:_URLOnStartLoading];
      break;
    case web::PAGE_LOADED:
      break;
  }
}

- (void)abortLoad {
  [self abortWebLoad];
  [self loadCancelled];
}

- (void)prepareForGoBack {
  // Make sure any transitions that may have occurred have been seen and acted
  // on by the CRWWebController, so the history stack and state of the
  // CRWWebController is 100% up to date before the stack navigation starts.
  if (self.webView) {
    [self injectEarlyInjectionScripts];
    [self checkForUnexpectedURLChange];
  }

  bool wasShowingInterstitial = _webStateImpl->IsShowingWebInterstitial();

  // Call into the delegate before |recordStateInHistory|.
  // TODO(rohitrao): Can this be reordered after |recordStateInHistory|?
  [_delegate webDidPrepareForGoBack];

  // Before changing the current session history entry, record the tab state.
  if (!wasShowingInterstitial) {
    [self recordStateInHistory];
  }
}

- (void)goBack {
  [self goDelta:-1];
}

- (void)goForward {
  [self goDelta:1];
}

- (void)goDelta:(int)delta {
  if (delta == 0) {
    [self reload];
    return;
  }

  // Abort if there is nothing next in the history.
  // Note that it is NOT checked that the history depth is at least |delta|.
  if ((delta < 0 && ![self canGoBack]) || (delta > 0 && ![self canGoForward])) {
    return;
  }

  if (delta < 0) {
    [self prepareForGoBack];
  } else {
    // Before changing the current session history entry, record the tab state.
    [self recordStateInHistory];
  }

  [_delegate webWillGoDelta:delta];

  CRWSessionController* sessionController =
      _webStateImpl->GetNavigationManagerImpl().GetSessionController();
  // fromEntry is retained because it has the potential to be released
  // by goDelta: if it has not been committed.
  base::scoped_nsobject<CRWSessionEntry> fromEntry(
      [[sessionController currentEntry] retain]);
  [sessionController goDelta:delta];
  if (fromEntry) {
    _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_HISTORY);
    [self finishHistoryNavigationFromEntry:fromEntry];
    _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_NORMAL);
  }
}

- (BOOL)isLoaded {
  return _loadPhase == web::PAGE_LOADED;
}

- (void)loadCompleteWithSuccess:(BOOL)loadSuccess {
  [self removePlaceholderOverlay];
  // The webView may have been torn down (or replaced by a native view). Be
  // safe and do nothing if that's happened.
  if (_loadPhase != web::PAGE_LOADING)
    return;

  DCHECK(self.webView);

  const GURL currentURL([self currentURL]);

  [self resetLoadState];
  _loadPhase = web::PAGE_LOADED;

  [self optOutScrollsToTopForSubviews];

  // Ensure the URL is as expected (and already reported to the delegate).
  DCHECK(currentURL == _lastRegisteredRequestURL)
      << std::endl
      << "currentURL = [" << currentURL << "]" << std::endl
      << "_lastRegisteredRequestURL = [" << _lastRegisteredRequestURL << "]";

  // Perform post-load-finished updates.
  [self didFinishWithURL:currentURL loadSuccess:loadSuccess];

  // Execute the pending LoadCompleteActions.
  for (ProceduralBlock action in _pendingLoadCompleteActions.get()) {
    action();
  }
  [_pendingLoadCompleteActions removeAllObjects];
}

- (void)didFinishWithURL:(const GURL&)currentURL loadSuccess:(BOOL)loadSuccess {
  DCHECK(_loadPhase == web::PAGE_LOADED);
  _webStateImpl->GetRequestTracker()->FinishPageLoad(currentURL, loadSuccess);
  // Reset the navigation type to the default value.
  // Note: it is possible that the web view has already started loading the
  // next page when this is called. In that case the cache mode can leak to
  // (some of) the requests of the next page. It's expected to be an edge case,
  // but if it becomes a problem it should be possible to notice it afterwards
  // and react to it (by warning the user or reloading the page for example).
  _webStateImpl->SetCacheMode(net::RequestTracker::CACHE_NORMAL);
  _webStateImpl->GetRequestTracker()->SetCacheModeFromUIThread(
      _webStateImpl->GetCacheMode());

  [self restoreStateFromHistory];
  [self loadCompletedForURL:currentURL];
  _webStateImpl->OnPageLoaded(currentURL, loadSuccess);
  _webStateImpl->SetIsLoading(false);
  // Inform the embedder the load completed.
  [_delegate webDidFinishWithURL:currentURL loadSuccess:loadSuccess];
}

- (void)finishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry {
  [_delegate webWillFinishHistoryNavigationFromEntry:fromEntry];

  // Only load the new URL if the current entry was not created by a JavaScript
  // window.history.pushState() call from |fromEntry|.
  BOOL shouldLoadURL =
      ![_webStateImpl->GetNavigationManagerImpl().GetSessionController()
          isPushStateNavigationBetweenEntry:fromEntry
                                   andEntry:self.currentSessionEntry];
  web::NavigationItemImpl* currentItem =
      self.currentSessionEntry.navigationItemImpl;
  GURL endURL = [self URLForHistoryNavigationFromItem:fromEntry.navigationItem
                                               toItem:currentItem];
  if (shouldLoadURL) {
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_RELOAD | ui::PAGE_TRANSITION_FORWARD_BACK);

    web::WebLoadParams params(endURL);
    if (currentItem) {
      params.referrer = currentItem->GetReferrer();
    }
    params.transition_type = transition;
    [self loadWithParams:params];
  }
  // Set the serialized state if necessary.  State must be set if the document
  // objects are the same. This can happen if:
  // - The navigation is a pushState (i.e., shouldLoadURL is NO).
  // - The navigation is a hash change.
  // TODO(crbug.com/566157): This misses some edge cases (e.g., a mixed series
  // of hash changes and push/replaceState calls will likely end up dispatching
  // this in cases where it shouldn't.
  if (!shouldLoadURL ||
      (web::GURLByRemovingRefFromGURL(endURL) ==
       web::GURLByRemovingRefFromGURL(fromEntry.navigationItem->GetURL()))) {
    NSString* stateObject = currentItem->GetSerializedStateObject();
    [self setPushedOrReplacedURL:currentItem->GetURL() stateObject:stateObject];
  }
}

- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem {
  const GURL& startURL = fromItem->GetURL();
  const GURL& endURL = toItem->GetURL();

  // Check the state of the fragments on both URLs (aka, is there a '#' in the
  // url or not).
  if (!startURL.has_ref() || endURL.has_ref()) {
    return endURL;
  }

  // startURL contains a fragment and endURL doesn't. Remove the fragment from
  // startURL and compare the resulting string to endURL. If they are equal, add
  // # to endURL to cause a hashchange event.
  GURL hashless = web::GURLByRemovingRefFromGURL(startURL);

  if (hashless != endURL)
    return endURL;

  url::StringPieceReplacements<std::string> emptyRef;
  emptyRef.SetRefStr("");
  GURL newEndURL = endURL.ReplaceComponents(emptyRef);
  toItem->SetURL(newEndURL);
  return newEndURL;
}

- (void)evaluateJavaScript:(NSString*)script
         JSONResultHandler:
             (void (^)(scoped_ptr<base::Value>, NSError*))handler {
  [self evaluateJavaScript:script
       stringResultHandler:^(NSString* stringResult, NSError* error) {
         DCHECK(stringResult || error);
         if (handler) {
           scoped_ptr<base::Value> result(
               base::JSONReader::Read(base::SysNSStringToUTF8(stringResult)));
           handler(std::move(result), error);
         }
       }];
}

- (void)addGestureRecognizerToWebView:(UIGestureRecognizer*)recognizer {
  if ([_gestureRecognizers containsObject:recognizer])
    return;

  [self.webView addGestureRecognizer:recognizer];
  [_gestureRecognizers addObject:recognizer];
}

- (void)removeGestureRecognizerFromWebView:(UIGestureRecognizer*)recognizer {
  if (![_gestureRecognizers containsObject:recognizer])
    return;

  [self.webView removeGestureRecognizer:recognizer];
  [_gestureRecognizers removeObject:recognizer];
}

- (void)addToolbarViewToWebView:(UIView*)toolbarView {
  DCHECK(toolbarView);
  if ([_webViewToolbars containsObject:toolbarView])
    return;
  [_webViewToolbars addObject:toolbarView];
  if (self.webView)
    [self.containerView addToolbar:toolbarView];
}

- (void)removeToolbarViewFromWebView:(UIView*)toolbarView {
  if (![_webViewToolbars containsObject:toolbarView])
    return;
  [_webViewToolbars removeObject:toolbarView];
  if (self.webView)
    [self.containerView removeToolbar:toolbarView];
}

- (CRWJSInjectionReceiver*)jsInjectionReceiver {
  return _jsInjectionReceiver;
}

- (BOOL)shouldClosePageOnNativeApplicationLoad {
  // The page should be closed if it was initiated by the DOM and there has been
  // no user interaction with the page since the web view was created, or if
  // the page has no navigation items, as occurs when an App Store link is
  // opened from another application.
  BOOL rendererInitiatedWithoutInteraction =
      self.sessionController.openedByDOM && !_userInteractedWithWebController;
  BOOL noNavigationItems =
      !_webStateImpl->GetNavigationManagerImpl().GetItemCount();
  return rendererInitiatedWithoutInteraction || noNavigationItems;
}

- (BOOL)isBeingDestroyed {
  return _isBeingDestroyed;
}

- (BOOL)isHalted {
  return _isHalted;
}

- (web::ReferrerPolicy)referrerPolicyFromString:(const std::string&)policy {
  // TODO(stuartmorgan): Remove this temporary bridge to the helper function
  // once the referrer handling moves into the subclasses.
  return web::ReferrerPolicyFromString(policy);
}

#pragma mark -
#pragma mark CRWWebControllerContainerViewDelegate

- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
        (CRWWebControllerContainerView*)containerView {
  return _webViewProxy.get();
}

- (CGFloat)headerHeightForContainerView:
        (CRWWebControllerContainerView*)containerView {
  return [self headerHeight];
}

#pragma mark -
#pragma mark CRWJSInjectionEvaluator Methods

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  // Subclasses must implement this method.
  NOTREACHED();
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  // Subclasses must implement this method.
  NOTREACHED();
  return NO;
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Make sure that CRWJSEarlyScriptManager has been injected.
  BOOL ealyScriptInjected =
      [self scriptHasBeenInjectedForClass:[CRWJSEarlyScriptManager class]
                           presenceBeacon:[_earlyScriptManager presenceBeacon]];
  if (!ealyScriptInjected &&
      JSInjectionManagerClass != [CRWJSEarlyScriptManager class]) {
    [_earlyScriptManager inject];
  }
}

- (web::WebViewType)webViewType {
  // Subclasses must implement this method.
  NOTREACHED();
  return web::UI_WEB_VIEW_TYPE;
}

#pragma mark -

- (void)evaluateUserJavaScript:(NSString*)script {
  // Subclasses must implement this method.
  NOTREACHED();
}

- (void)didFinishNavigation {
  // This can be called at multiple times after the document has loaded. Do
  // nothing if the document has already loaded.
  if (_loadPhase == web::PAGE_LOADED)
    return;
  [self loadCompleteWithSuccess:YES];
}

- (BOOL)respondToMessage:(base::DictionaryValue*)message
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL {
  std::string command;
  if (!message->GetString("command", &command)) {
    DLOG(WARNING) << "JS message parameter not found: command";
    return NO;
  }

  SEL handler = [self selectorToHandleJavaScriptCommand:command];
  if (!handler) {
    if (!self.webStateImpl->OnScriptCommandReceived(
            command, *message, originURL, userIsInteracting)) {
      // Message was either unexpected or not correctly handled.
      // Page is reset as a precaution.
      DLOG(WARNING) << "Unexpected message received: " << command;
      return NO;
    }
    return YES;
  }

  typedef BOOL (*HandlerType)(id, SEL, base::DictionaryValue*, NSDictionary*);
  HandlerType handlerImplementation =
      reinterpret_cast<HandlerType>([self methodForSelector:handler]);
  DCHECK(handlerImplementation);
  NSMutableDictionary* context =
      [NSMutableDictionary dictionaryWithObject:@(userIsInteracting)
                                         forKey:web::kUserIsInteractingKey];
  NSURL* originNSURL = net::NSURLWithGURL(originURL);
  if (originNSURL)
    context[web::kOriginURLKey] = originNSURL;
  return handlerImplementation(self, handler, message, context);
}

- (SEL)selectorToHandleJavaScriptCommand:(const std::string&)command {
  static std::map<std::string, SEL>* handlers = nullptr;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    handlers = new std::map<std::string, SEL>();
    (*handlers)["addPluginPlaceholders"] =
        @selector(handleAddPluginPlaceholdersMessage:context:);
    (*handlers)["chrome.send"] = @selector(handleChromeSendMessage:context:);
    (*handlers)["console"] = @selector(handleConsoleMessage:context:);
    (*handlers)["dialog.suppressed"] =
        @selector(handleDialogSuppressedMessage:context:);
    (*handlers)["dialog.willShow"] =
        @selector(handleDialogWillShowMessage:context:);
    (*handlers)["document.favicons"] =
        @selector(handleDocumentFaviconsMessage:context:);
    (*handlers)["document.retitled"] =
        @selector(handleDocumentRetitledMessage:context:);
    (*handlers)["document.submit"] =
        @selector(handleDocumentSubmitMessage:context:);
    (*handlers)["externalRequest"] =
        @selector(handleExternalRequestMessage:context:);
    (*handlers)["form.activity"] =
        @selector(handleFormActivityMessage:context:);
    (*handlers)["navigator.credentials.request"] =
        @selector(handleCredentialsRequestedMessage:context:);
    (*handlers)["navigator.credentials.notifySignedIn"] =
        @selector(handleSignedInMessage:context:);
    (*handlers)["navigator.credentials.notifySignedOut"] =
        @selector(handleSignedOutMessage:context:);
    (*handlers)["navigator.credentials.notifyFailedSignIn"] =
        @selector(handleSignInFailedMessage:context:);
    (*handlers)["resetExternalRequest"] =
        @selector(handleResetExternalRequestMessage:context:);
    (*handlers)["window.close.self"] =
        @selector(handleWindowCloseSelfMessage:context:);
    (*handlers)["window.error"] = @selector(handleWindowErrorMessage:context:);
    (*handlers)["window.hashchange"] =
        @selector(handleWindowHashChangeMessage:context:);
    (*handlers)["window.history.back"] =
        @selector(handleWindowHistoryBackMessage:context:);
    (*handlers)["window.history.willChangeState"] =
        @selector(handleWindowHistoryWillChangeStateMessage:context:);
    (*handlers)["window.history.didPushState"] =
        @selector(handleWindowHistoryDidPushStateMessage:context:);
    (*handlers)["window.history.didReplaceState"] =
        @selector(handleWindowHistoryDidReplaceStateMessage:context:);
    (*handlers)["window.history.forward"] =
        @selector(handleWindowHistoryForwardMessage:context:);
    (*handlers)["window.history.go"] =
        @selector(handleWindowHistoryGoMessage:context:);
  });
  DCHECK(handlers);
  auto iter = handlers->find(command);
  return iter != handlers->end() ? iter->second : nullptr;
}

#pragma mark -
#pragma mark JavaScript message handlers

- (BOOL)handleAddPluginPlaceholdersMessage:(base::DictionaryValue*)message
                                   context:(NSDictionary*)context {
  // Inject the script that adds the plugin placeholders.
  [[_jsInjectionReceiver
      instanceOfClass:[CRWJSPluginPlaceholderManager class]] inject];
  return YES;
}

- (BOOL)handleChromeSendMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context {
  if (_webStateImpl->HasWebUI()) {
    const GURL currentURL([self currentURL]);
    if (web::GetWebClient()->IsAppSpecificURL(currentURL)) {
      std::string messageContent;
      base::ListValue* arguments = nullptr;
      if (!message->GetString("message", &messageContent)) {
        DLOG(WARNING) << "JS message parameter not found: message";
        return NO;
      }
      if (!message->GetList("arguments", &arguments)) {
        DLOG(WARNING) << "JS message parameter not found: arguments";
        return NO;
      }
      _webStateImpl->OnScriptCommandReceived(
          messageContent, *message, currentURL,
          context[web::kUserIsInteractingKey]);
      _webStateImpl->ProcessWebUIMessage(currentURL, messageContent,
                                         *arguments);
      return YES;
    }
  }

  DLOG(WARNING)
      << "chrome.send message not handled because WebUI was not found.";
  return NO;
}

- (BOOL)handleConsoleMessage:(base::DictionaryValue*)message
                     context:(NSDictionary*)context {
  // Do not log if JS logging is off.
  if (![[NSUserDefaults standardUserDefaults] boolForKey:web::kLogJavaScript]) {
    return YES;
  }

  std::string method;
  if (!message->GetString("method", &method)) {
    DLOG(WARNING) << "JS message parameter not found: method";
    return NO;
  }
  std::string consoleMessage;
  if (!message->GetString("message", &consoleMessage)) {
    DLOG(WARNING) << "JS message parameter not found: message";
    return NO;
  }
  std::string origin;
  if (!message->GetString("origin", &origin)) {
    DLOG(WARNING) << "JS message parameter not found: origin";
    return NO;
  }

  DVLOG(0) << origin << " [" << method << "] " << consoleMessage;
  return YES;
}

- (BOOL)handleDialogSuppressedMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  if ([_delegate
          respondsToSelector:@selector(webControllerDidSuppressDialog:)]) {
    [_delegate webControllerDidSuppressDialog:self];
  }
  return YES;
}

- (BOOL)handleDialogWillShowMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context {
  if ([_delegate respondsToSelector:@selector(webControllerWillShowDialog:)]) {
    [_delegate webControllerWillShowDialog:self];
  }
  return YES;
}

- (BOOL)handleDocumentFaviconsMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  base::ListValue* favicons = nullptr;
  if (!message->GetList("favicons", &favicons)) {
    DLOG(WARNING) << "JS message parameter not found: favicons";
    return NO;
  }
  std::vector<web::FaviconURL> urls;
  for (size_t fav_idx = 0; fav_idx != favicons->GetSize(); ++fav_idx) {
    base::DictionaryValue* favicon = nullptr;
    if (!favicons->GetDictionary(fav_idx, &favicon))
      return NO;
    std::string href;
    std::string rel;
    if (!favicon->GetString("href", &href)) {
      DLOG(WARNING) << "JS message parameter not found: href";
      return NO;
    }
    if (!favicon->GetString("rel", &rel)) {
      DLOG(WARNING) << "JS message parameter not found: rel";
      return NO;
    }
    web::FaviconURL::IconType icon_type = web::FaviconURL::FAVICON;
    if (rel == "apple-touch-icon")
      icon_type = web::FaviconURL::TOUCH_ICON;
    else if (rel == "apple-touch-icon-precomposed")
      icon_type = web::FaviconURL::TOUCH_PRECOMPOSED_ICON;
    urls.push_back(
        web::FaviconURL(GURL(href), icon_type, std::vector<gfx::Size>()));
  }
  if (!urls.empty())
    _webStateImpl->OnFaviconUrlUpdated(urls);
  return YES;
}

- (BOOL)handleDocumentSubmitMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context {
  std::string href;
  if (!message->GetString("href", &href)) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return NO;
  }
  const GURL targetURL(href);
  const GURL currentURL([self currentURL]);
  bool targetsFrame = false;
  message->GetBoolean("targetsFrame", &targetsFrame);
  if (!targetsFrame && web::UrlHasWebScheme(targetURL)) {
    // The referrer is not known yet, and will be updated later.
    const web::Referrer emptyReferrer;
    [self registerLoadRequest:targetURL
                     referrer:emptyReferrer
                   transition:ui::PAGE_TRANSITION_FORM_SUBMIT];
  }
  std::string formName;
  message->GetString("formName", &formName);
  base::scoped_nsobject<NSSet> observers([_observers copy]);
  // We decide the form is user-submitted if the user has interacted with
  // the main page (using logic from the popup blocker), or if the keyboard
  // is visible.
  BOOL submittedByUser = [context[web::kUserIsInteractingKey] boolValue] ||
                         [_webViewProxy keyboardAccessory];
  _webStateImpl->OnDocumentSubmitted(formName, submittedByUser);
  return YES;
}

- (BOOL)handleExternalRequestMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  std::string href;
  std::string target;
  std::string referrerPolicy;
  if (!message->GetString("href", &href)) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return NO;
  }
  if (!message->GetString("target", &target)) {
    DLOG(WARNING) << "JS message parameter not found: target";
    return NO;
  }
  if (!message->GetString("referrerPolicy", &referrerPolicy)) {
    DLOG(WARNING) << "JS message parameter not found: referrerPolicy";
    return NO;
  }
  // Round-trip the href through NSURL; this URL will be compared as a
  // string against a UIWebView-provided NSURL later, and must match exactly
  // for the new window to trigger, so the escaping needs to be NSURL-style.
  // TODO(stuartmorgan): Comparing against a URL whose exact formatting we
  // don't control is fundamentally fragile; try to find another
  // way of handling this.
  DCHECK(context[web::kUserIsInteractingKey]);
  NSString* windowName =
      base::SysUTF8ToNSString(href + web::kWindowNameSeparator + target);
  _externalRequest.reset(new web::NewWindowInfo(
      net::GURLWithNSURL(net::NSURLWithGURL(GURL(href))), windowName,
      web::ReferrerPolicyFromString(referrerPolicy),
      [context[web::kUserIsInteractingKey] boolValue]));
  return YES;
}

- (BOOL)handleFormActivityMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context {
  std::string formName;
  std::string fieldName;
  std::string type;
  std::string value;
  int keyCode = web::WebStateObserver::kInvalidFormKeyCode;
  bool inputMissing = false;
  if (!message->GetString("formName", &formName) ||
      !message->GetString("fieldName", &fieldName) ||
      !message->GetString("type", &type) ||
      !message->GetString("value", &value)) {
    inputMissing = true;
  }

  if (!message->GetInteger("keyCode", &keyCode) || keyCode < 0)
    keyCode = web::WebStateObserver::kInvalidFormKeyCode;
  _webStateImpl->OnFormActivityRegistered(formName, fieldName, type, value,
                                          keyCode, inputMissing);
  return YES;
}

- (BOOL)handleCredentialsRequestedMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context {
  int request_id = -1;
  if (!message->GetInteger("requestId", &request_id)) {
    DLOG(WARNING) << "JS message parameter not found: requestId";
    return NO;
  }
  bool suppress_ui = false;
  if (!message->GetBoolean("suppressUI", &suppress_ui)) {
    DLOG(WARNING) << "JS message parameter not found: suppressUI";
    return NO;
  }
  base::ListValue* federations_value = nullptr;
  if (!message->GetList("federations", &federations_value)) {
    DLOG(WARNING) << "JS message parameter not found: federations";
    return NO;
  }
  std::vector<std::string> federations;
  for (auto federation_value : *federations_value) {
    std::string federation;
    if (!federation_value->GetAsString(&federation)) {
      DLOG(WARNING) << "JS message parameter 'federations' contains wrong type";
      return NO;
    }
    federations.push_back(federation);
  }
  DCHECK(context[web::kUserIsInteractingKey]);
  _webStateImpl->OnCredentialsRequested(
      request_id, net::GURLWithNSURL(context[web::kOriginURLKey]), suppress_ui,
      federations, [context[web::kUserIsInteractingKey] boolValue]);
  return YES;
}

- (BOOL)handleSignedInMessage:(base::DictionaryValue*)message
                      context:(NSDictionary*)context {
  int request_id = -1;
  if (!message->GetInteger("requestId", &request_id)) {
    DLOG(WARNING) << "JS message parameter not found: requestId";
    return NO;
  }
  base::DictionaryValue* credential_data = nullptr;
  web::Credential credential;
  if (message->GetDictionary("credential", &credential_data)) {
    if (!web::DictionaryValueToCredential(*credential_data, &credential)) {
      DLOG(WARNING) << "JS message parameter 'credential' is invalid";
      return NO;
    }
    _webStateImpl->OnSignedIn(request_id,
                              net::GURLWithNSURL(context[web::kOriginURLKey]),
                              credential);
  } else {
    _webStateImpl->OnSignedIn(request_id,
                              net::GURLWithNSURL(context[web::kOriginURLKey]));
  }
  return YES;
}

- (BOOL)handleSignedOutMessage:(base::DictionaryValue*)message
                       context:(NSDictionary*)context {
  int request_id = -1;
  if (!message->GetInteger("requestId", &request_id)) {
    DLOG(WARNING) << "JS message parameter not found: requestId";
    return NO;
  }
  _webStateImpl->OnSignedOut(request_id,
                             net::GURLWithNSURL(context[web::kOriginURLKey]));
  return YES;
}

- (BOOL)handleSignInFailedMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context {
  int request_id = -1;
  if (!message->GetInteger("requestId", &request_id)) {
    DLOG(WARNING) << "JS message parameter not found: requestId";
    return NO;
  }
  base::DictionaryValue* credential_data = nullptr;
  web::Credential credential;
  if (message->GetDictionary("credential", &credential_data)) {
    if (!web::DictionaryValueToCredential(*credential_data, &credential)) {
      DLOG(WARNING) << "JS message parameter 'credential' is invalid";
      return NO;
    }
    _webStateImpl->OnSignInFailed(
        request_id, net::GURLWithNSURL(context[web::kOriginURLKey]),
        credential);
  } else {
    _webStateImpl->OnSignInFailed(
        request_id, net::GURLWithNSURL(context[web::kOriginURLKey]));
  }
  return YES;
}

- (BOOL)handleResetExternalRequestMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context {
  _externalRequest.reset();
  return YES;
}

- (BOOL)handleWindowCloseSelfMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  [self orderClose];
  return YES;
}

- (BOOL)handleWindowErrorMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context {
  std::string errorMessage;
  if (!message->GetString("message", &errorMessage)) {
    DLOG(WARNING) << "JS message parameter not found: message";
    return NO;
  }
  DLOG(ERROR) << "JavaScript error: " << errorMessage
              << " URL:" << [self currentURL].spec();
  return YES;
}

- (BOOL)handleWindowHashChangeMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  [self checkForUnexpectedURLChange];

  // Because hash changes don't trigger |-didFinishNavigation|, fetch favicons
  // for the new page manually.
  [self evaluateJavaScript:@"__gCrWeb.sendFaviconsToHost();"
       stringResultHandler:nil];

  // Notify the observers.
  _webStateImpl->OnUrlHashChanged();
  return YES;
}

- (BOOL)handleWindowHistoryBackMessage:(base::DictionaryValue*)message
                               context:(NSDictionary*)context {
  [self goBack];
  return YES;
}

- (BOOL)handleWindowHistoryForwardMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context {
  [self goForward];
  return YES;
}

- (BOOL)handleWindowHistoryGoMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  int delta;
  message->GetInteger("value", &delta);
  [self goDelta:delta];
  return YES;
}

- (BOOL)handleWindowHistoryWillChangeStateMessage:(base::DictionaryValue*)unused
                                          context:(NSDictionary*)unusedContext {
  // This dummy handler is a workaround for crbug.com/490673. Issue was
  // happening when two sequential calls of window.history.pushState were
  // performed by the page. In that case state was changed twice before
  // first change was reported to embedder (and first URL change was reported
  // incorrectly).

  // Using dummy handler for window.history.willChangeState message holds
  // second state change until the first change is reported, because messages
  // are queued. This is essentially a sleep, and not the real fix of the
  // problem. TODO(eugenebut): refactor handleWindowHistoryDidPushStateMessage:
  // to avoid this "sleep".
  return YES;
}

- (BOOL)handleWindowHistoryDidPushStateMessage:(base::DictionaryValue*)message
                                       context:(NSDictionary*)context {
  // If there is a pending entry, a new navigation has been registered but
  // hasn't begun loading.  Since the pushState message is coming from the
  // previous page, ignore it and allow the previously registered navigation to
  // continue.  This can ocur if a pushState is issued from an anchor tag
  // onClick event, as the click would have already been registered.
  if ([self sessionController].pendingEntry)
    return NO;

  std::string pageURL;
  std::string baseURL;
  if (!message->GetString("pageUrl", &pageURL) ||
      !message->GetString("baseUrl", &baseURL)) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return NO;
  }
  GURL pushURL = web::history_state_util::GetHistoryStateChangeUrl(
      [self currentURL], GURL(baseURL), pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  pushURL = URLEscapedForHistory(pushURL);
  if (!pushURL.is_valid())
    return YES;
  web::NavigationItem* navItem = [self currentNavItem];
  // PushState happened before first navigation entry or called when the
  // navigation entry does not contain a valid URL.
  if (!navItem || !navItem->GetURL().is_valid())
    return YES;
  if (!web::history_state_util::IsHistoryStateChangeValid(navItem->GetURL(),
                                                          pushURL)) {
    // A redirect may have occurred just prior to the pushState. Check if
    // the URL needs to be updated.
    // TODO(bdibello): Investigate how the pushState() is handled before the
    // redirect and after core.js injection.
    [self checkForUnexpectedURLChange];
  }
  if (!web::history_state_util::IsHistoryStateChangeValid(
          [self currentNavItem]->GetURL(), pushURL)) {
    // If the current session entry URL origin still doesn't match pushURL's
    // origin, ignore the pushState. This can happen if a new URL is loaded
    // just before the pushState.
    return YES;
  }
  std::string stateObjectJSON;
  if (!message->GetString("stateObject", &stateObjectJSON)) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return NO;
  }
  NSString* stateObject = base::SysUTF8ToNSString(stateObjectJSON);
  _URLOnStartLoading = pushURL;
  _lastRegisteredRequestURL = pushURL;

  // If the user interacted with the page, categorize it as a link navigation.
  // If not, categorize it is a client redirect as it occurred without user
  // input and should not be added to the history stack.
  // TODO(crbug.com/549301): Improve transition detection.
  ui::PageTransition transition = self.userInteractionRegistered
                                      ? ui::PAGE_TRANSITION_LINK
                                      : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  [self pushStateWithPageURL:pushURL
                 stateObject:stateObject
                  transition:transition];

  NSString* replaceWebViewJS =
      [self javascriptToReplaceWebViewURL:pushURL stateObjectJSON:stateObject];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self evaluateJavaScript:replaceWebViewJS
       stringResultHandler:^(NSString*, NSError*) {
         if (!weakSelf || weakSelf.get()->_isBeingDestroyed)
           return;
         base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
         [strongSelf optOutScrollsToTopForSubviews];
         // Notify the observers.
         strongSelf.get()->_webStateImpl->OnHistoryStateChanged();
         [strongSelf didFinishNavigation];
       }];
  return YES;
}

- (BOOL)handleWindowHistoryDidReplaceStateMessage:
    (base::DictionaryValue*)message
                                          context:(NSDictionary*)context {
  std::string pageURL;
  std::string baseURL;
  if (!message->GetString("pageUrl", &pageURL) ||
      !message->GetString("baseUrl", &baseURL)) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return NO;
  }
  GURL replaceURL = web::history_state_util::GetHistoryStateChangeUrl(
      [self currentURL], GURL(baseURL), pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  replaceURL = URLEscapedForHistory(replaceURL);
  if (!replaceURL.is_valid())
    return YES;
  const NavigationManagerImpl& navigationManager =
      _webStateImpl->GetNavigationManagerImpl();
  web::NavigationItem* navItem = [self currentNavItem];
  // ReplaceState happened before first navigation entry or called right
  // after window.open when the url is empty/not valid.
  if (!navItem ||
      (navigationManager.GetItemCount() <= 1 && navItem->GetURL().is_empty()))
    return YES;
  if (!web::history_state_util::IsHistoryStateChangeValid(navItem->GetURL(),
                                                          replaceURL)) {
    // A redirect may have occurred just prior to the replaceState. Check if
    // the URL needs to be updated.
    [self checkForUnexpectedURLChange];
  }
  if (!web::history_state_util::IsHistoryStateChangeValid(
          [self currentNavItem]->GetURL(), replaceURL)) {
    // If the current session entry URL origin still doesn't match
    // replaceURL's origin, ignore the replaceState. This can happen if a
    // new URL is loaded just before the replaceState.
    return YES;
  }
  std::string stateObjectJSON;
  if (!message->GetString("stateObject", &stateObjectJSON)) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return NO;
  }
  NSString* stateObject = base::SysUTF8ToNSString(stateObjectJSON);
  _URLOnStartLoading = replaceURL;
  _lastRegisteredRequestURL = replaceURL;
  [self replaceStateWithPageURL:replaceURL stateObject:stateObject];
  NSString* replaceStateJS = [self javascriptToReplaceWebViewURL:replaceURL
                                                 stateObjectJSON:stateObject];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self evaluateJavaScript:replaceStateJS
       stringResultHandler:^(NSString*, NSError*) {
         if (!weakSelf || weakSelf.get()->_isBeingDestroyed)
           return;
         base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
         [strongSelf didFinishNavigation];
       }];
  return YES;
}

#pragma mark -

- (BOOL)wantsKeyboardShield {
  if ([self.nativeController
          respondsToSelector:@selector(wantsKeyboardShield)]) {
    return [self.nativeController wantsKeyboardShield];
  }
  return YES;
}

- (BOOL)wantsLocationBarHintText {
  if ([self.nativeController
          respondsToSelector:@selector(wantsLocationBarHintText)]) {
    return [self.nativeController wantsLocationBarHintText];
  }
  return YES;
}

// TODO(stuartmorgan): This method conflates document changes and URL changes;
// we should be distinguishing better, and be clear about the expected
// WebDelegate and WCO callbacks in each case.
- (void)webPageChanged {
  DCHECK(_loadPhase == web::LOAD_REQUESTED);

  const GURL currentURL([self currentURL]);
  web::Referrer referrer = [self currentReferrer];
  // If no referrer was known in advance, record it now. (If there was one,
  // keep it since it will have a more accurate URL and policy than what can
  // be extracted from the landing page.)
  web::NavigationItem* currentItem = [self currentNavItem];
  if (!currentItem->GetReferrer().url.is_valid()) {
    currentItem->SetReferrer(referrer);
  }

  // TODO(stuartmorgan): This shouldn't be called for hash state or
  // push/replaceState.
  [self resetDocumentSpecificState];

  [self didStartLoadingURL:currentURL updateHistory:YES];
}

- (void)resetDocumentSpecificState {
  _lastUserInteraction.reset();
  _clickInProgress = NO;
  _lastSeenWindowID.reset([[_windowIDJSManager windowId] copy]);
}

- (void)didStartLoadingURL:(const GURL&)url updateHistory:(BOOL)updateHistory {
  _loadPhase = web::PAGE_LOADING;
  _URLOnStartLoading = url;
  _displayStateOnStartLoading = self.pageDisplayState;

  self.userInteractionRegistered = NO;
  _pageHasZoomed = NO;

  [[self sessionController] commitPendingEntry];
  _webStateImpl->GetRequestTracker()->StartPageLoad(
      url, [[self sessionController] currentEntry]);
  [_delegate webDidStartLoadingURL:url shouldUpdateHistory:updateHistory];
}

- (BOOL)checkForUnexpectedURLChange {
  // Subclasses may override this method to check for and handle URL changes.
  return NO;
}

- (void)wasShown {
  if ([self.nativeController respondsToSelector:@selector(wasShown)]) {
    [self.nativeController wasShown];
  }
}

- (void)wasHidden {
  if (_isHalted)
    return;
  if ([self.nativeController respondsToSelector:@selector(wasHidden)]) {
    [self.nativeController wasHidden];
  }
}

+ (BOOL)webControllerCanShow:(const GURL&)url {
  return web::UrlHasWebScheme(url) ||
         web::GetWebClient()->IsAppSpecificURL(url) ||
         url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kAboutScheme);
}

- (void)setUserInteractionRegistered:(BOOL)flag {
  _userInteractionRegistered = flag;
}

- (BOOL)userInteractionRegistered {
  return _userInteractionRegistered;
}

- (BOOL)useDesktopUserAgent {
  web::NavigationItem* item = [self currentNavItem];
  return item && item->IsOverridingUserAgent();
}

- (void)cachePOSTDataForRequest:(NSURLRequest*)request
                 inSessionEntry:(CRWSessionEntry*)currentSessionEntry {
  NSUInteger maxPOSTDataSizeInBytes = 4096;
  NSString* cookieHeaderName = @"cookie";

  web::NavigationItemImpl* currentItem = currentSessionEntry.navigationItemImpl;
  DCHECK(currentItem);
  const bool shouldUpdateEntry =
      ui::PageTransitionCoreTypeIs(currentItem->GetTransitionType(),
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) &&
      ![request HTTPBodyStream] &&  // Don't cache streams.
      !currentItem->HasPostData() &&
      currentItem->GetURL() == net::GURLWithNSURL([request URL]);
  const bool belowSizeCap =
      [[request HTTPBody] length] < maxPOSTDataSizeInBytes;
  DLOG_IF(WARNING, shouldUpdateEntry && !belowSizeCap)
      << "Data in POST request exceeds the size cap (" << maxPOSTDataSizeInBytes
      << " bytes), and will not be cached.";

  if (shouldUpdateEntry && belowSizeCap) {
    currentItem->SetPostData([request HTTPBody]);
    currentItem->ResetHttpRequestHeaders();
    currentItem->AddHttpRequestHeaders([request allHTTPHeaderFields]);
    // Don't cache the "Cookie" header.
    // According to NSURLRequest documentation, |-valueForHTTPHeaderField:| is
    // case insensitive, so it's enough to test the lower case only.
    if ([request valueForHTTPHeaderField:cookieHeaderName]) {
      // Case insensitive search in |headers|.
      NSSet* cookieKeys = [currentItem->GetHttpRequestHeaders()
          keysOfEntriesPassingTest:^(id key, id obj, BOOL* stop) {
            NSString* header = (NSString*)key;
            const BOOL found =
                [header caseInsensitiveCompare:cookieHeaderName] ==
                NSOrderedSame;
            *stop = found;
            return found;
          }];
      DCHECK_EQ(1u, [cookieKeys count]);
      currentItem->RemoveHttpRequestHeaderForKey([cookieKeys anyObject]);
    }
  }
}

// TODO(stuartmorgan): This is mostly logic from the original UIWebView delegate
// method, which provides less information than the WKWebView version. Audit
// this for things that should be handled in the subclass instead.
- (BOOL)shouldAllowLoadWithRequest:(NSURLRequest*)request
                       targetFrame:(const web::FrameInfo*)targetFrame
                       isLinkClick:(BOOL)isLinkClick {
  GURL requestURL = net::GURLWithNSURL(request.URL);

  // Check if the request should be delayed.
  if (_externalRequest && _externalRequest->url == requestURL) {
    // Links that can't be shown in a tab by Chrome but can be handled by
    // external apps (e.g. tel:, mailto:) are opened directly despite the target
    // attribute on the link. We don't open a new tab for them because Mobile
    // Safari doesn't do that (and sites are expecting us to do the same) and
    // also because there would be nothing shown in that new tab; it would
    // remain on about:blank (see crbug.com/240178)
    if ([CRWWebController webControllerCanShow:requestURL] ||
        ![_delegate openExternalURL:requestURL]) {
      web::NewWindowInfo windowInfo = *_externalRequest;
      dispatch_async(dispatch_get_main_queue(), ^{
        [self openPopupWithInfo:windowInfo];
      });
    }
    _externalRequest.reset();
    return NO;
  }

  BOOL shouldCheckNativeApp = [self shouldClosePageOnNativeApplicationLoad];

  // Check if the link navigation leads to a launch of an external app.
  // TODO(shreyasv): Change this such that handling/stealing of link navigations
  // is delegated to the WebDelegate and the logic around external app launching
  // is moved there as well.
  if (shouldCheckNativeApp || isLinkClick) {
    // Check If the URL is handled by a native app.
    if ([self urlTriggersNativeAppLaunch:requestURL
                               sourceURL:[self currentNavigationURL]]) {
      // External app has been launched successfully. Stop the current page
      // load operation (e.g. notifying all observers) and record the URL so
      // that errors reported following the 'NO' reply can be safely ignored.
      if ([self shouldClosePageOnNativeApplicationLoad])
        [_delegate webPageOrderedClose];
      [self stopLoading];
      [_openedApplicationURL addObject:request.URL];
      return NO;
    }
  }

  // The WebDelegate may instruct the CRWWebController to stop loading, and
  // instead instruct the next page to be loaded in an animation.
  GURL mainDocumentURL = net::GURLWithNSURL(request.mainDocumentURL);
  DCHECK(self.webView);
  if (![self shouldOpenURL:requestURL
           mainDocumentURL:mainDocumentURL
               linkClicked:isLinkClick]) {
    return NO;
  }

  // If the URL doesn't look like one we can show, try to open the link with an
  // external application.
  // TODO(droger):  Check transition type before opening an external
  // application? For example, only allow it for TYPED and LINK transitions.
  if (![CRWWebController webControllerCanShow:requestURL]) {
    if (![self shouldOpenExternalURLRequest:request targetFrame:targetFrame]) {
      return NO;
    }

    // Stop load if navigation is believed to be happening on the main frame.
    if ([self isPutativeMainFrameRequest:request targetFrame:targetFrame])
      [self stopLoading];

    if ([_delegate openExternalURL:requestURL]) {
      // Record the URL so that errors reported following the 'NO' reply can be
      // safely ignored.
      [_openedApplicationURL addObject:request.URL];
      if ([self shouldClosePageOnNativeApplicationLoad])
        [_delegate webPageOrderedClose];
    }
    return NO;
  }

  if ([[request HTTPMethod] isEqualToString:@"POST"]) {
    CRWSessionEntry* currentEntry = [self currentSessionEntry];
    // TODO(crbug.com/570699): Remove this check once it's no longer possible to
    // have no current entries.
    if (currentEntry)
      [self cachePOSTDataForRequest:request inSessionEntry:currentEntry];
  }

  return YES;
}

- (void)restoreStateAfterURLRejection {
  [[self sessionController] discardNonCommittedEntries];

  // Reset |_lastRegisteredRequestURL| so that it reflects the URL from before
  // the load was rejected. This value may be out of sync because
  // |_lastRegisteredRequestURL| may have already been updated before the load
  // was rejected.
  _lastRegisteredRequestURL = [self currentURL];
  _loadPhase = web::PAGE_LOADING;
  [self didFinishNavigation];
}

- (void)handleLoadError:(NSError*)error inMainFrame:(BOOL)inMainFrame {
  if ([error code] == NSURLErrorUnsupportedURL)
    return;
  // In cases where a Plug-in handles the load do not take any further action.
  if ([error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      (error.code == web::kWebKitErrorPlugInLoadFailed ||
       error.code == web::kWebKitErrorCannotShowUrl))
    return;

  // Continue processing only if the error is on the main request or is the
  // result of a user interaction.
  NSDictionary* userInfo = [error userInfo];
  // |userinfo| contains the request creation date as a NSDate.
  NSTimeInterval requestCreationDate =
      [[userInfo objectForKey:@"CreationDate"] timeIntervalSinceReferenceDate];
  bool userInteracted = false;
  if (requestCreationDate != 0.0 && _lastUserInteraction) {
    NSTimeInterval timeSinceInteraction =
        requestCreationDate - _lastUserInteraction->time;
    // The error is considered to be the result of a user interaction if any
    // interaction happened just before the request was made.
    // TODO(droger): If the user interacted with the page after the request was
    // made (i.e. creationTimeSinceLastInteraction < 0), then
    // |_lastUserInteraction| has been overridden. The current behavior is to
    // discard the interstitial in that case. A better decision could be made if
    // we had a history of all the user interactions instead of just the last
    // one.
    userInteracted =
        timeSinceInteraction < kMaximumDelayForUserInteractionInSeconds &&
        _lastUserInteraction->time > _lastTransferTimeInSeconds &&
        timeSinceInteraction >= 0.0;
  } else {
    // If the error does not have timing information, check if the user
    // interacted with the page recently.
    userInteracted = [self userIsInteracting];
  }
  if (!inMainFrame && !userInteracted)
    return;

  NSURL* errorURL = [NSURL
      URLWithString:[userInfo objectForKey:NSURLErrorFailingURLStringErrorKey]];
  const GURL errorGURL = net::GURLWithNSURL(errorURL);

  // Handles Frame Load Interrupted errors from WebView.
  if ([error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
    // See if the delegate wants to handle this case.
    if (errorGURL.is_valid() &&
        [_delegate
            respondsToSelector:@selector(
                                   controllerForUnhandledContentAtURL:)]) {
      id<CRWNativeContent> controller =
          [_delegate controllerForUnhandledContentAtURL:errorGURL];
      if (controller) {
        [self loadCompleteWithSuccess:NO];
        [self removeWebViewAllowingCachedReconstruction:NO];
        [self setNativeController:controller];
        [self loadNativeViewWithSuccess:YES];
        return;
      }
    }

    // Ignore errors that originate from URLs that are opened in external apps.
    if ([_openedApplicationURL containsObject:errorURL])
      return;
    // Certain frame errors don't have URL information for some reason; for
    // those cases (so far the only known case is plugin content loaded directly
    // in a frame) just ignore the error. See crbug.com/414295
    if (!errorURL) {
      DCHECK(!inMainFrame);
      return;
    }
    // The wrapper error uses the URL of the error and not the requested URL
    // (which can be different in case of a redirect) to match desktop Chrome
    // behavior.
    NSError* wrapperError = [NSError
        errorWithDomain:[error domain]
                   code:[error code]
               userInfo:@{
                 NSURLErrorFailingURLStringErrorKey : [errorURL absoluteString],
                 NSUnderlyingErrorKey : error
               }];
    [self loadCompleteWithSuccess:NO];
    [self loadErrorInNativeView:wrapperError];
    return;
  }

  if ([error code] == NSURLErrorCancelled) {
    [self handleCancelledError:error];
    // NSURLErrorCancelled errors that aren't handled by aborting the load will
    // automatically be retried by the web view, so early return in this case.
    return;
  }

  [self loadCompleteWithSuccess:NO];
  [self loadErrorInNativeView:error];
}

- (void)handleCancelledError:(NSError*)cancelledError {
  // Subclasses must implement this method.
  NOTREACHED();
}

#pragma mark -
#pragma mark WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  _webStateImpl->CreateWebUI(URL);
}

- (void)clearWebUI {
  _webStateImpl->ClearWebUI();
}

#pragma mark -
#pragma mark UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Allows the custom UILongPressGestureRecognizer to fire simultaneously with
  // other recognizers.
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);
  if (![self supportsCustomContextMenu]) {
    // Fetching context menu info is not a free operation, early return if a
    // context menu should not be shown.
    return YES;
  }

  // This is custom long press gesture recognizer. By the time the gesture is
  // recognized the web controller needs to know if there is a link under the
  // touch. If there a link, the web controller will reject system's context
  // menu and show another one. If for some reason context menu info is not
  // fetched - system context menu will be shown.
  [self setDOMElementForLastTouch:nullptr];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self fetchDOMElementAtPoint:[touch locationInView:self.webView]
             completionHandler:^(scoped_ptr<base::DictionaryValue> element) {
               [weakSelf setDOMElementForLastTouch:std::move(element)];
             }];
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);
  if (!self.webView || ![self supportsCustomContextMenu]) {
    // Show the context menu iff currently displaying a web view.
    // Do nothing for native views.
    return NO;
  }

  // Fetching is considered as successful even if |_DOMElementForLastTouch| is
  // empty. However if |_DOMElementForLastTouch| is empty then custom context
  // menu should not be shown.
  UMA_HISTOGRAM_BOOLEAN("WebController.FetchContextMenuInfoAsyncSucceeded",
                        _DOMElementForLastTouch);
  return _DOMElementForLastTouch && !_DOMElementForLastTouch->empty();
}

#pragma mark -
#pragma mark CRWRequestTrackerDelegate

- (BOOL)isForStaticFileRequests {
  return NO;
}

- (void)updatedSSLStatus:(const web::SSLStatus&)sslStatus
              forPageUrl:(const GURL&)url
                userInfo:(id)userInfo {
  // |userInfo| is a CRWSessionEntry.
  web::NavigationItem* item =
      [static_cast<CRWSessionEntry*>(userInfo) navigationItem];
  if (!item)
    return;  // This is a request update for an entry that no longer exists.

  // This condition happens infrequently when a page load is misinterpreted as
  // a resource load from a previous page. This can happen when moving quickly
  // back and forth through history, the notifications from the web view on the
  // UI thread and the one from the requests at the net layer may get out of
  // sync. This catches this case and prevent updating an entry with the wrong
  // SSL data.
  if (item->GetURL().GetOrigin() != url.GetOrigin())
    return;

  if (item->GetSSL().Equals(sslStatus))
    return;  // No need to update with the same data.

  item->GetSSL() = sslStatus;

  // Notify the UI it needs to refresh if the updated entry is the current
  // entry.
  if (userInfo == self.currentSessionEntry) {
    [self didUpdateSSLStatusForCurrentNavigationItem];
  }
}

- (void)handleResponseHeaders:(net::HttpResponseHeaders*)headers
                   requestUrl:(const GURL&)requestUrl {
  _webStateImpl->OnHttpResponseHeadersReceived(headers, requestUrl);
}

- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
                  onUrl:(const GURL&)url
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue {
  DCHECK(_delegate);
  DCHECK_EQ(url, [self currentNavigationURL]);
  [_delegate presentSSLError:info
                forSSLStatus:status
                 recoverable:recoverable
                    callback:^(BOOL proceed) {
                      if (proceed) {
                        // The interstitial will be removed during reload.
                        [self loadCurrentURL];
                      }
                      if (shouldContinue) {
                        shouldContinue(proceed);
                      }
                    }];
  DCHECK([self currentNavItem]);
  [self currentNavItem]->SetUnsafe(true);
}

- (void)updatedProgress:(float)progress {
  if ([_delegate
          respondsToSelector:@selector(webController:didUpdateProgress:)]) {
    [_delegate webController:self didUpdateProgress:progress];
  }
}

- (void)certificateUsed:(net::X509Certificate*)certificate
                forHost:(const std::string&)host
                 status:(net::CertStatus)status {
  [[[self sessionController] sessionCertificatePolicyManager]
      registerAllowedCertificate:certificate
                         forHost:host
                          status:status];
}

- (void)clearCertificates {
  [[[self sessionController] sessionCertificatePolicyManager]
      clearCertificates];
}

#pragma mark -
#pragma mark Popup handling

- (void)openPopupWithInfo:(const web::NewWindowInfo&)windowInfo {
  const GURL url(windowInfo.url);
  const GURL currentURL([self currentNavigationURL]);
  NSString* windowName = windowInfo.window_name.get();
  web::Referrer referrer(currentURL, windowInfo.referrer_policy);
  base::WeakNSObject<CRWWebController> weakSelf(self);
  void (^showPopupHandler)() = ^{
    CRWWebController* child = [[weakSelf delegate] webPageOrderedOpen:url
                                                             referrer:referrer
                                                           windowName:windowName
                                                         inBackground:NO];
    DCHECK(!child || child.sessionController.openedByDOM);
  };

  BOOL showPopup = windowInfo.user_is_interacting ||
                   (![self shouldBlockPopupWithURL:url sourceURL:currentURL]);
  if (showPopup) {
    showPopupHandler();
  } else if ([_delegate
                 respondsToSelector:@selector(webController:didBlockPopup:)]) {
    web::BlockedPopupInfo blockedPopupInfo(url, referrer, windowName,
                                           showPopupHandler);
    [_delegate webController:self didBlockPopup:blockedPopupInfo];
  }
}

#pragma mark -
#pragma mark TouchTracking

- (void)touched:(BOOL)touched {
  _clickInProgress = touched;
  if (touched) {
    self.userInteractionRegistered = YES;
    _userInteractedWithWebController = YES;
    if (_isBeingDestroyed)
      return;
    const web::NavigationManagerImpl& navigationManager =
        self.webStateImpl->GetNavigationManagerImpl();
    GURL mainDocumentURL =
        navigationManager.GetItemCount()
            ? navigationManager.GetLastCommittedItem()->GetURL()
            : [self currentURL];
    _lastUserInteraction.reset(new web::UserInteractionEvent(mainDocumentURL));
  }
}

- (CRWTouchTrackingRecognizer*)touchTrackingRecognizer {
  if (!_touchTrackingRecognizer) {
    _touchTrackingRecognizer.reset(
        [[CRWTouchTrackingRecognizer alloc] initWithDelegate:self]);
  }
  return _touchTrackingRecognizer.get();
}

- (BOOL)userIsInteracting {
  // If page transfer started after last click, user is deemed to be no longer
  // interacting.
  if (!_lastUserInteraction ||
      _lastTransferTimeInSeconds > _lastUserInteraction->time) {
    return NO;
  }
  return [self userClickedRecently];
}

- (BOOL)userClickedRecently {
  if (!_lastUserInteraction)
    return NO;
  return _clickInProgress ||
         ((CFAbsoluteTimeGetCurrent() - _lastUserInteraction->time) <
          kMaximumDelayForUserInteractionInSeconds);
}

#pragma mark Placeholder Overlay Methods

- (void)addPlaceholderOverlay {
  if (!_overlayPreviewMode) {
    // Create |kSnapshotOverlayDelay| second timer to remove image with
    // transition.
    [self performSelector:@selector(removePlaceholderOverlay)
               withObject:nil
               afterDelay:kSnapshotOverlayDelay];
  }

  // Add overlay image.
  _placeholderOverlayView.reset([[UIImageView alloc] init]);
  CGRect frame = [self visibleFrame];
  [_placeholderOverlayView setFrame:frame];
  [_placeholderOverlayView
      setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                          UIViewAutoresizingFlexibleHeight];
  [_placeholderOverlayView setContentMode:UIViewContentModeScaleAspectFill];
  [self.containerView addSubview:_placeholderOverlayView];

  id callback = ^(UIImage* image) {
    [_placeholderOverlayView setImage:image];
  };
  [_delegate webController:self retrievePlaceholderOverlayImage:callback];

  if (!_placeholderOverlayView.get().image) {
    // TODO(shreyasv): This is just a blank white image. Consider adding an API
    // so that the delegate can return something immediately for the default
    // overlay image.
    _placeholderOverlayView.get().image = [[self class] defaultSnapshotImage];
  }
}

- (void)removePlaceholderOverlay {
  if (!_placeholderOverlayView || _overlayPreviewMode)
    return;

  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(removeOverlay)
                                             object:nil];
  // Remove overlay with transition.
  [UIView animateWithDuration:kSnapshotOverlayTransition
      animations:^{
        [_placeholderOverlayView setAlpha:0.0f];
      }
      completion:^(BOOL finished) {
        [_placeholderOverlayView removeFromSuperview];
        _placeholderOverlayView.reset();
      }];
}

- (void)setOverlayPreviewMode:(BOOL)overlayPreviewMode {
  _overlayPreviewMode = overlayPreviewMode;

  // If we were showing the preview, remove it.
  if (!_overlayPreviewMode && _placeholderOverlayView) {
    [self resetContainerView];
    // Reset |_placeholderOverlayView| directly instead of calling
    // -removePlaceholderOverlay, which removes |_placeholderOverlayView| in an
    // animation.
    [_placeholderOverlayView removeFromSuperview];
    _placeholderOverlayView.reset();
    // There are cases when resetting the contentView, above, may happen after
    // the web view has been created. Re-add it here, rather than
    // relying on a subsequent call to loadCurrentURLInWebView.
    if (self.webView) {
      [[self view] addSubview:self.webView];
    }
  }
}

- (void)internalSuppressDialogs:(BOOL)suppressFlag notify:(BOOL)notifyFlag {
  NSString* const kSetSuppressDialogs =
      [NSString stringWithFormat:@"__gCrWeb.setSuppressDialogs(%d, %d);",
                                 suppressFlag, notifyFlag];
  [self setSuppressDialogsWithHelperScript:kSetSuppressDialogs];
}

- (void)setPageDialogOpenPolicy:(web::PageDialogOpenPolicy)policy {
  switch (policy) {
    case web::DIALOG_POLICY_ALLOW:
      [self setSuppressDialogs:NO notify:NO];
      return;
    case web::DIALOG_POLICY_NOTIFY_FIRST:
      [self setSuppressDialogs:NO notify:YES];
      return;
    case web::DIALOG_POLICY_SUPPRESS:
      [self setSuppressDialogs:YES notify:YES];
      return;
  }
  NOTREACHED();
}

- (void)setSuppressDialogs:(BOOL)suppressFlag notify:(BOOL)notifyFlag {
  if (self.webView && [_earlyScriptManager hasBeenInjected]) {
    [self internalSuppressDialogs:suppressFlag notify:notifyFlag];
  } else {
    _setSuppressDialogsLater = suppressFlag;
    _setNotifyAboutDialogsLater = notifyFlag;
  }
}

#pragma mark -
#pragma mark Session Information

- (CRWSessionController*)sessionController {
  return _webStateImpl
      ? _webStateImpl->GetNavigationManagerImpl().GetSessionController()
      : nil;
}

- (CRWSessionEntry*)currentSessionEntry {
  return [[self sessionController] currentEntry];
}

- (web::NavigationItem*)currentNavItem {
  // This goes through the legacy Session* interface rather than Navigation*
  // because it is itself a legacy method that should not exist, and this
  // avoids needing to add a GetActiveItem to NavigationManager. If/when this
  // method chain becomes a blocker to eliminating SessionController, the logic
  // can be moved here, using public NavigationManager getters. That's not
  // done now in order to avoid code duplication.
  return [[self currentSessionEntry] navigationItem];
}

- (const GURL&)currentNavigationURL {
  // TODO(stuartmorgan): Fix the fact that this method doesn't have clear usage
  // delination that would allow changing to one of the non-deprecated URL
  // calls.
  web::NavigationItem* item = [self currentNavItem];
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

- (ui::PageTransition)currentTransition {
  if ([self currentNavItem])
    return [self currentNavItem]->GetTransitionType();
  else
    return ui::PageTransitionFromInt(0);
}

- (web::Referrer)currentSessionEntryReferrer {
  web::NavigationItem* currentItem = [self currentNavItem];
  return currentItem ? currentItem->GetReferrer() : web::Referrer();
}

- (NSDictionary*)currentHTTPHeaders {
  DCHECK([self currentSessionEntry]);
  return [self currentSessionEntry].navigationItem->GetHttpRequestHeaders();
}

#pragma mark -
#pragma mark CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidZoom:
        (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  _pageHasZoomed = YES;
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  web::NavigationItem* currentItem = [self currentNavItem];
  if (webViewScrollViewProxy.isZooming || _applyingPageState || !currentItem)
    return;
  CGSize contentSize = webViewScrollViewProxy.contentSize;
  if (contentSize.width < CGRectGetWidth(webViewScrollViewProxy.frame)) {
    // The renderer incorrectly resized the content area.  Resetting the scroll
    // view's zoom scale will force a re-rendering.  rdar://23963992
    _applyingPageState = YES;
    web::PageZoomState zoomState =
        currentItem->GetPageDisplayState().zoom_state();
    if (!zoomState.IsValid() || zoomState.IsLegacyFormat())
      zoomState = web::PageZoomState(1.0, 1.0, 1.0);
    [self applyWebViewScrollZoomScaleFromZoomState:zoomState];
    _applyingPageState = NO;
  }
}
#pragma mark -
#pragma mark Page State

- (void)recordStateInHistory {
  // Only record the state if:
  // - the current NavigationItem's URL matches the current URL, and
  // - the user has interacted with the page.
  CRWSessionEntry* current = [self currentSessionEntry];
  if (current && [current navigationItem]->GetURL() == [self currentURL] &&
      self.userInteractionRegistered) {
    [current navigationItem]->SetPageDisplayState(self.pageDisplayState);
  }
}

- (void)restoreStateFromHistory {
  CRWSessionEntry* current = [self currentSessionEntry];
  if ([current navigationItem])
    self.pageDisplayState = [current navigationItem]->GetPageDisplayState();
}

- (web::PageDisplayState)pageDisplayState {
  web::PageDisplayState displayState;
  if (self.webView) {
    CGPoint scrollOffset = [self scrollPosition];
    displayState.scroll_state().set_offset_x(std::floor(scrollOffset.x));
    displayState.scroll_state().set_offset_y(std::floor(scrollOffset.y));
    UIScrollView* scrollView = self.webScrollView;
    displayState.zoom_state().set_minimum_zoom_scale(
        scrollView.minimumZoomScale);
    displayState.zoom_state().set_maximum_zoom_scale(
        scrollView.maximumZoomScale);
    displayState.zoom_state().set_zoom_scale(scrollView.zoomScale);
  } else {
    // TODO(crbug.com/546146): Handle native views.
  }
  return displayState;
}

- (void)setPageDisplayState:(web::PageDisplayState)displayState {
  if (!displayState.IsValid())
    return;
  if (self.webView) {
    // Page state is restored after a page load completes.  If the user has
    // scrolled or changed the zoom scale while the page is still loading, don't
    // restore any state since it will confuse the user.
    web::PageDisplayState currentPageDisplayState = self.pageDisplayState;
    if (currentPageDisplayState.scroll_state().offset_x() ==
            _displayStateOnStartLoading.scroll_state().offset_x() &&
        currentPageDisplayState.scroll_state().offset_y() ==
            _displayStateOnStartLoading.scroll_state().offset_y() &&
        !_pageHasZoomed) {
      [self applyPageDisplayState:displayState];
    }
  }
}

- (void)orientationDidChange {
  // When rotating, the available zoom scale range may change, zoomScale's
  // percentage into this range should remain constant.  However, there are
  // two known bugs with respect to adjusting the zoomScale on rotation:
  // - WKWebView sometimes erroneously resets the scroll view's zoom scale to
  // an incorrect value ( rdar://20100815 ).
  // - After zooming occurs in a UIWebView that's displaying a page with a hard-
  // coded viewport width, the zoom will not be updated upon rotation
  // ( crbug.com/485055 ).
  if (!self.webView)
    return;
  web::NavigationItem* currentItem = self.currentNavItem;
  if (!currentItem)
    return;
  web::PageDisplayState displayState = currentItem->GetPageDisplayState();
  if (!displayState.IsValid())
    return;
  CGFloat zoomPercentage = (displayState.zoom_state().zoom_scale() -
                            displayState.zoom_state().minimum_zoom_scale()) /
                           displayState.zoom_state().GetMinMaxZoomDifference();
  displayState.zoom_state().set_minimum_zoom_scale(
      self.webScrollView.minimumZoomScale);
  displayState.zoom_state().set_maximum_zoom_scale(
      self.webScrollView.maximumZoomScale);
  displayState.zoom_state().set_zoom_scale(
      displayState.zoom_state().minimum_zoom_scale() +
      zoomPercentage * displayState.zoom_state().GetMinMaxZoomDifference());
  currentItem->SetPageDisplayState(displayState);
  [self applyPageDisplayState:currentItem->GetPageDisplayState()];
}

- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState {
  if (!displayState.IsValid())
    return;
  base::WeakNSObject<CRWWebController> weakSelf(self);
  web::PageDisplayState displayStateCopy = displayState;
  [self queryUserScalableProperty:^(BOOL isUserScalable) {
    base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
    [strongSelf applyPageDisplayState:displayStateCopy
                         userScalable:isUserScalable];
  }];
}

- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState
                 userScalable:(BOOL)isUserScalable {
  // Early return if |scrollState| doesn't match the current NavigationItem.
  // This can sometimes occur in tests, as navigation occurs programmatically
  // and |-applyPageScrollState:| is asynchronous.
  web::NavigationItem* currentItem = [self currentSessionEntry].navigationItem;
  if (currentItem && currentItem->GetPageDisplayState() != displayState)
    return;
  DCHECK(displayState.IsValid());
  _applyingPageState = YES;
  if (isUserScalable) {
    [self prepareToApplyWebViewScrollZoomScale];
    [self applyWebViewScrollZoomScaleFromZoomState:displayState.zoom_state()];
    [self finishApplyingWebViewScrollZoomScale];
  }
  [self applyWebViewScrollOffsetFromScrollState:displayState.scroll_state()];
  _applyingPageState = NO;
}

- (void)prepareToApplyWebViewScrollZoomScale {
  id webView = self.webView;
  if (![webView respondsToSelector:@selector(viewForZoomingInScrollView:)]) {
    return;
  }

  UIView* contentView = [webView viewForZoomingInScrollView:self.webScrollView];

  if ([webView
          respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]) {
    [webView scrollViewWillBeginZooming:self.webScrollView
                               withView:contentView];
  }
}

- (void)finishApplyingWebViewScrollZoomScale {
  id webView = self.webView;
  if ([webView respondsToSelector:@selector(scrollViewDidEndZooming:
                                                           withView:
                                                            atScale:)] &&
      [webView respondsToSelector:@selector(viewForZoomingInScrollView:)]) {
    // This correctly sets the content's frame in the scroll view to
    // fit the web page and upscales the content so that it isn't
    // blurry.
    UIView* contentView =
        [webView viewForZoomingInScrollView:self.webScrollView];
    [webView scrollViewDidEndZooming:self.webScrollView
                            withView:contentView
                             atScale:self.webScrollView.zoomScale];
  }
}

- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState {
  // Subclasses must implement this method.
  NOTREACHED();
}

- (void)applyWebViewScrollOffsetFromScrollState:
    (const web::PageScrollState&)scrollState {
  DCHECK(scrollState.IsValid());
  CGPoint scrollOffset =
      CGPointMake(scrollState.offset_x(), scrollState.offset_y());
  if (_loadPhase == web::PAGE_LOADED) {
    // If the page is loaded, update the scroll immediately.
    [self.webScrollView setContentOffset:scrollOffset];
  } else {
    // If the page isn't loaded, store the action to update the scroll
    // when the page finishes loading.
    base::WeakNSObject<UIScrollView> weakScrollView(self.webScrollView);
    base::scoped_nsprotocol<ProceduralBlock> action([^{
      [weakScrollView setContentOffset:scrollOffset];
    } copy]);
    [_pendingLoadCompleteActions addObject:action];
  }
}

#pragma mark -
#pragma mark Web Page Features

// TODO(eugenebut): move JS parsing code to a separate file.
- (void)queryUserScalableProperty:(void (^)(BOOL))responseHandler {
  NSString* const kViewPortContentQuery =
      @"var viewport = document.querySelector('meta[name=\"viewport\"]');"
       "viewport ? viewport.content : '';";
  [self evaluateJavaScript:kViewPortContentQuery
       stringResultHandler:^(NSString* viewPortContent, NSError* error) {
         responseHandler(
             GetUserScalablePropertyFromViewPortContent(viewPortContent));
       }];
}

- (void)fetchWebPageWidthWithCompletionHandler:(void (^)(CGFloat))handler {
  if (!self.webView) {
    handler(0);
    return;
  }

  [self evaluateJavaScript:@"__gCrWeb.getPageWidth();"
       stringResultHandler:^(NSString* pageWidthAsString, NSError*) {
         handler([pageWidthAsString floatValue]);
       }];
}

- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:
                 (void (^)(scoped_ptr<base::DictionaryValue>))handler {
  DCHECK(handler);
  // Convert point into web page's coordinate system (which may be scaled and/or
  // scrolled).
  CGPoint scrollOffset = self.scrollPosition;
  CGFloat webViewContentWidth = self.webScrollView.contentSize.width;
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self fetchWebPageWidthWithCompletionHandler:^(CGFloat pageWidth) {
    CGFloat scale = pageWidth / webViewContentWidth;
    CGPoint localPoint = CGPointMake((point.x + scrollOffset.x) * scale,
                                     (point.y + scrollOffset.y) * scale);
    NSString* const kGetElementScript =
        [NSString stringWithFormat:@"__gCrWeb.getElementFromPoint(%g, %g);",
                                   localPoint.x, localPoint.y];
    [weakSelf evaluateJavaScript:kGetElementScript
               JSONResultHandler:^(scoped_ptr<base::Value> element, NSError*) {
                 // Release raw element and call handler with DictionaryValue.
                 scoped_ptr<base::DictionaryValue> elementAsDict;
                 if (element) {
                   base::DictionaryValue* elementAsDictPtr = nullptr;
                   element.release()->GetAsDictionary(&elementAsDictPtr);
                   // |rawElement| and |elementPtr| now point to the same
                   // memory. |elementPtr| ownership will be transferred to
                   // |element| scoped_ptr.
                   elementAsDict.reset(elementAsDictPtr);
                 }
                 handler(std::move(elementAsDict));
               }];
  }];
}

- (NSDictionary*)contextMenuInfoForElement:(base::DictionaryValue*)element {
  DCHECK(element);
  NSMutableDictionary* mutableInfo = [NSMutableDictionary dictionary];
  NSString* title = nil;
  std::string href;
  if (element->GetString("href", &href)) {
    mutableInfo[web::kContextLinkURLString] = base::SysUTF8ToNSString(href);
    GURL linkURL(href);
    if (linkURL.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      DCHECK(web::GetWebClient());
      const std::string& acceptLangs = web::GetWebClient()->GetAcceptLangs(
          self.webStateImpl->GetBrowserState());
      base::string16 urlText =
          url_formatter::FormatUrl(GURL(href), acceptLangs);
      title = base::SysUTF16ToNSString(urlText);
    }
  }
  std::string src;
  if (element->GetString("src", &src)) {
    mutableInfo[web::kContextImageURLString] = base::SysUTF8ToNSString(src);
    if (!title)
      title = base::SysUTF8ToNSString(src);
    if ([title hasPrefix:@"data:"])
      title = @"";
  }
  std::string titleAttribute;
  if (element->GetString("title", &titleAttribute))
    title = base::SysUTF8ToNSString(titleAttribute);
  std::string referrerPolicy;
  element->GetString("referrerPolicy", &referrerPolicy);
  mutableInfo[web::kContextLinkReferrerPolicy] =
      @([self referrerPolicyFromString:referrerPolicy]);
  if (title)
    mutableInfo[web::kContextTitle] = title;
  return [[mutableInfo copy] autorelease];
}

#pragma mark -
#pragma mark Fullscreen

- (CGRect)visibleFrame {
  CGRect frame = self.containerView.bounds;
  CGFloat headerHeight = [self headerHeight];
  frame.origin.y = headerHeight;
  frame.size.height -= headerHeight;
  return frame;
}

- (void)optOutScrollsToTopForSubviews {
  NSMutableArray* stack =
      [NSMutableArray arrayWithArray:[self.webScrollView subviews]];
  while (stack.count) {
    UIView* current = [stack lastObject];
    [stack removeLastObject];
    [stack addObjectsFromArray:[current subviews]];
    if ([current isKindOfClass:[UIScrollView class]])
      static_cast<UIScrollView*>(current).scrollsToTop = NO;
  }
}

#pragma mark -
#pragma mark WebDelegate Calls

- (BOOL)shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  if (![_delegate respondsToSelector:@selector(webController:
                                               shouldOpenURL:
                                             mainDocumentURL:
                                                 linkClicked:)]) {
    return YES;
  }
  return [_delegate webController:self
                    shouldOpenURL:url
                  mainDocumentURL:mainDocumentURL
                      linkClicked:linkClicked];
}

- (BOOL)isPutativeMainFrameRequest:(NSURLRequest*)request
                       targetFrame:(const web::FrameInfo*)targetFrame {
  // Determine whether the request is for the main frame using frame info if
  // available. In the case of missing frame info, the request is considered to
  // have originated from the main frame if either of the following is true:
  //   (a) The request's URL matches the request's main document URL
  //   (b) The request's URL resourceSpecifier matches the request's
  //       mainDocumentURL specifier, as is the case upon redirect from http
  //       App Store links to a URL with itms-apps scheme. This appears to be is
  //       App Store specific behavior, specially handled by web view.
  // Note: These heuristics are not guaranteed to be correct, and should not be
  // used for any decisions with security implications.
  return targetFrame
             ? targetFrame->is_main_frame
             : [request.URL isEqual:request.mainDocumentURL] ||
                   [request.URL.resourceSpecifier
                       isEqual:request.mainDocumentURL.resourceSpecifier];
}

- (BOOL)shouldOpenExternalURLRequest:(NSURLRequest*)request
                         targetFrame:(const web::FrameInfo*)targetFrame {
  ExternalURLRequestStatus requestStatus = NUM_EXTERNAL_URL_REQUEST_STATUS;
  if ([self isPutativeMainFrameRequest:request targetFrame:targetFrame]) {
    requestStatus = MAIN_FRAME_ALLOWED;
  } else {
    // If the request's main document URL differs from that at the time of the
    // last user interaction, then the page has changed since the user last
    // interacted.
    BOOL userInteractedWithRequestMainFrame =
        [self userClickedRecently] &&
        net::GURLWithNSURL(request.mainDocumentURL) ==
            _lastUserInteraction->main_document_url;
    // Prevent subframe requests from opening an external URL if the user has
    // not interacted with the request's main frame.
    requestStatus = userInteractedWithRequestMainFrame ? SUBFRAME_ALLOWED
                                                       : SUBFRAME_BLOCKED;
  }
  DCHECK_NE(requestStatus, NUM_EXTERNAL_URL_REQUEST_STATUS);
  UMA_HISTOGRAM_ENUMERATION("WebController.ExternalURLRequestBlocking",
                            requestStatus, NUM_EXTERNAL_URL_REQUEST_STATUS);
  if (requestStatus == SUBFRAME_BLOCKED) {
    return NO;
  }

  GURL requestURL = net::GURLWithNSURL(request.URL);
  return [_delegate respondsToSelector:@selector(webController:
                                           shouldOpenExternalURL:)] &&
         [_delegate webController:self shouldOpenExternalURL:requestURL];
}

- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)url
                         sourceURL:(const GURL&)sourceURL {
  return
      [_delegate respondsToSelector:@selector(urlTriggersNativeAppLaunch:
                                                               sourceURL:)] &&
      [_delegate urlTriggersNativeAppLaunch:url sourceURL:sourceURL];
}

- (CGFloat)headerHeight {
  if (![_delegate respondsToSelector:@selector(headerHeightForWebController:)])
    return 0.0f;
  return [_delegate headerHeightForWebController:self];
}

- (BOOL)shouldBlockPopupWithURL:(const GURL&)popupURL
                      sourceURL:(const GURL&)sourceURL {
  if (![_delegate respondsToSelector:@selector(webController:
                                         shouldBlockPopupWithURL:
                                                       sourceURL:)]) {
    return NO;
  }
  return [_delegate webController:self
          shouldBlockPopupWithURL:popupURL
                        sourceURL:sourceURL];
}

- (void)didUpdateHistoryStateWithPageURL:(const GURL&)url {
  _webStateImpl->GetRequestTracker()->HistoryStateChange(url);
  [_delegate webDidUpdateHistoryStateWithPageURL:url];
}

- (void)didUpdateSSLStatusForCurrentNavigationItem {
  if ([_delegate respondsToSelector:
          @selector(
              webControllerDidUpdateSSLStatusForCurrentNavigationItem:)]) {
    [_delegate webControllerDidUpdateSSLStatusForCurrentNavigationItem:self];
  }
}

#pragma mark CRWWebControllerScripting Methods

- (void)loadHTML:(NSString*)html {
  [self loadHTML:html forURL:GURL(url::kAboutBlankURL)];
}

- (void)loadHTMLForCurrentURL:(NSString*)html {
  [self loadHTML:html forURL:self.currentURL];
}

- (void)loadHTML:(NSString*)html forURL:(const GURL&)url {
  // Remove the transient content view.
  [self clearTransientContentView];

  DLOG_IF(WARNING, !self.webView)
      << "self.webView null while trying to load HTML";
  _loadPhase = web::LOAD_REQUESTED;
  [self loadWebHTMLString:html forURL:url];
}

- (void)loadHTML:(NSString*)HTML forAppSpecificURL:(const GURL&)URL {
  CHECK(web::GetWebClient()->IsAppSpecificURL(URL));
  [self loadHTML:HTML forURL:URL];
}

- (void)stopLoading {
  web::RecordAction(UserMetricsAction("Stop"));
  // Discard the pending and transient entried before notifying the tab model
  // observers of the change via |-abortLoad|.
  [[self sessionController] discardNonCommittedEntries];
  [self abortLoad];
  // If discarding the non-committed entries results in an app-specific URL,
  // reload it in its native view.
  if (!self.nativeController &&
      [self shouldLoadURLInNativeView:[self currentNavigationURL]]) {
    [self loadCurrentURLInNativeView];
  }
}

- (void)orderClose {
  if (self.sessionController.openedByDOM) {
    [_delegate webPageOrderedClose];
  }
}

#pragma mark -
#pragma mark Testing-Only Methods

- (void)injectWebViewContentView:(id)webViewContentView {
  [self removeWebViewAllowingCachedReconstruction:NO];

  _lastRegisteredRequestURL = _defaultURL;
  [self.containerView displayWebViewContentView:webViewContentView];
}

- (void)resetInjectedWebViewContentView {
  [self resetWebView];
  [self resetContainerView];
}

- (void)addObserver:(id<CRWWebControllerObserver>)observer {
  DCHECK(observer);
  if (!_observers) {
    // We don't want our observer set to block dealloc on the observers. For the
    // observer container, make an object compatible with NSMutableSet that does
    // not perform retain or release on the contained objects (weak references).
    CFSetCallBacks callbacks =
        {0, NULL, NULL, CFCopyDescription, CFEqual, CFHash};
    _observers.reset(base::mac::CFToNSCast(
        CFSetCreateMutable(kCFAllocatorDefault, 1, &callbacks)));
  }
  DCHECK(![_observers containsObject:observer]);
  [_observers addObject:observer];
  _observerBridges.push_back(
      new web::WebControllerObserverBridge(observer, self.webStateImpl, self));

  if ([observer respondsToSelector:@selector(setWebViewProxy:controller:)])
    [observer setWebViewProxy:_webViewProxy controller:self];
}

- (void)removeObserver:(id<CRWWebControllerObserver>)observer {
  // TODO(jimblackler): make _observers use NSMapTable. crbug.com/367992
  DCHECK([_observers containsObject:observer]);
  [_observers removeObject:observer];
  // Remove the associated WebControllerObserverBridge.
  auto it = std::find_if(_observerBridges.begin(), _observerBridges.end(),
                         [observer](web::WebControllerObserverBridge* bridge) {
                           return bridge->web_controller_observer() == observer;
                         });
  DCHECK(it != _observerBridges.end());
  _observerBridges.erase(it);
}

- (NSUInteger)observerCount {
  DCHECK_EQ(_observerBridges.size(), [_observers count]);
  return [_observers count];
}

- (NSString*)windowId {
  return [_windowIDJSManager windowId];
}

- (void)setWindowId:(NSString*)windowId {
  return [_windowIDJSManager setWindowId:windowId];
}

- (NSString*)lastSeenWindowID {
  return _lastSeenWindowID;
}

- (void)setURLOnStartLoading:(const GURL&)url {
  _URLOnStartLoading = url;
}

- (const GURL&)defaultURL {
  return _defaultURL;
}

- (GURL)URLOnStartLoading {
  return _URLOnStartLoading;
}

- (GURL)lastRegisteredRequestURL {
  return _lastRegisteredRequestURL;
}

- (void)simulateLoadRequestWithURL:(const GURL&)URL {
  _lastRegisteredRequestURL = URL;
  _loadPhase = web::LOAD_REQUESTED;
}

- (NSString*)externalRequestWindowName {
  if (!_externalRequest || !_externalRequest->window_name)
    return @"";
  return _externalRequest->window_name;
}

- (void)resetExternalRequest {
  _externalRequest.reset();
}

- (web::WebViewDocumentType)documentTypeFromMIMEType:(NSString*)MIMEType {
  if (!MIMEType.length) {
    return web::WEB_VIEW_DOCUMENT_TYPE_UNKNOWN;
  }

  if ([MIMEType isEqualToString:@"text/html"] ||
      [MIMEType isEqualToString:@"application/xhtml+xml"] ||
      [MIMEType isEqualToString:@"application/xml"]) {
    return web::WEB_VIEW_DOCUMENT_TYPE_HTML;
  }

  return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
}

@end
