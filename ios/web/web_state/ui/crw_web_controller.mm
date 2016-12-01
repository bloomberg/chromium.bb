// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#import <objc/runtime.h>
#include <stddef.h>

#include <cmath>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/mru_cache.h"
#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#import "base/ios/ns_error_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/net/http_response_headers_util.h"
#import "ios/net/nsurlrequest_util.h"
#import "ios/web/crw_network_activity_indicator_manager.h"
#import "ios/web/history_state_util.h"
#include "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/net/cert_host_pair.h"
#import "ios/web/net/crw_cert_verification_controller.h"
#import "ios/web/net/crw_ssl_status_updater.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/java_script_dialog_presenter.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/origin_util.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/referrer_util.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_kit_constants.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "ios/web/public/web_state/credential.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/credential_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/page_display_state.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/crw_pass_kit_downloader.h"
#import "ios/web/web_state/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/error_translation_util.h"
#import "ios/web/web_state/js/crw_js_plugin_placeholder_manager.h"
#import "ios/web/web_state/js/crw_js_post_request_loader.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/page_viewport_state.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_controller_observer_bridge.h"
#include "ios/web/web_state/web_state_facade_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "ios/web/webui/crw_web_ui_manager.h"
#import "ios/web/webui/mojo_facade.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::UserMetricsAction;
using web::NavigationManager;
using web::NavigationManagerImpl;
using web::WebState;
using web::WebStateImpl;

namespace web {
NSString* const kContainerViewID = @"Container View";
const char* kWindowNameSeparator = "#";
NSString* const kUserIsInteractingKey = @"userIsInteracting";
NSString* const kOriginURLKey = @"originURL";
NSString* const kLogJavaScript = @"LogJavascript";

struct NewWindowInfo {
  GURL url;
  base::scoped_nsobject<NSString> window_name;
  web::ReferrerPolicy referrer_policy;
  bool user_is_interacting;
  NewWindowInfo(GURL url,
                NSString* window_name,
                web::ReferrerPolicy referrer_policy,
                bool user_is_interacting);
  ~NewWindowInfo();
};

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

// Values of the UMA |Web.URLVerificationFailure| histogram.
enum WebViewDocumentType {
  // Generic contents (e.g. PDF documents).
  WEB_VIEW_DOCUMENT_TYPE_GENERIC = 0,
  // HTML contents.
  WEB_VIEW_DOCUMENT_TYPE_HTML,
  // Unknown contents.
  WEB_VIEW_DOCUMENT_TYPE_UNKNOWN,
  WEB_VIEW_DOCUMENT_TYPE_COUNT,
};

}  // namespace web

namespace {

// Key of UMA IOSFix.ViewportZoomBugCount histogram.
const char kUMAViewportZoomBugCount[] = "Renderer.ViewportZoomBugCount";

// A tag for the web view, so that tests can identify it. This is used instead
// of exposing a getter (and deliberately not exposed in the header) to make it
// *very* clear that this is a hack which should only be used as a last resort.
const NSUInteger kWebViewTag = 0x3eb71e3;
// URL scheme for messages sent from javascript for asynchronous processing.
NSString* const kScriptMessageName = @"crwebinvoke";

// Constants for storing the source of NSErrors received by WKWebViews:
// - Errors received by |-webView:didFailProvisionalNavigation:withError:| are
//   recorded using WKWebViewErrorSource::PROVISIONAL_LOAD.  These should be
//   cancelled.
// - Errors received by |-webView:didFailNavigation:withError:| are recorded
//   using WKWebViewsource::NAVIGATION.  These errors should not be cancelled,
//   as the WKWebView will automatically retry the load.
NSString* const kWKWebViewErrorSourceKey = @"ErrorSource";
typedef enum { NONE = 0, PROVISIONAL_LOAD, NAVIGATION } WKWebViewErrorSource;

// Represents cert verification error, which happened inside
// |webView:didReceiveAuthenticationChallenge:completionHandler:| and should
// be checked inside |webView:didFailProvisionalNavigation:withError:|.
struct CertVerificationError {
  CertVerificationError(BOOL is_recoverable, net::CertStatus status)
      : is_recoverable(is_recoverable), status(status) {}

  BOOL is_recoverable;
  net::CertStatus status;
};

// Type of Cache object for storing cert verification errors.
typedef base::MRUCache<web::CertHostPair, CertVerificationError>
    CertVerificationErrorsCacheType;

// Maximum number of errors to store in cert verification errors cache.
// Cache holds errors only for pending navigations, so the actual number of
// stored errors is not expected to be high.
const CertVerificationErrorsCacheType::size_type kMaxCertErrorsCount = 100;

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

// Utility function for getting the source of NSErrors received by WKWebViews.
WKWebViewErrorSource WKWebViewErrorSourceFromError(NSError* error) {
  DCHECK(error);
  return static_cast<WKWebViewErrorSource>(
      [error.userInfo[kWKWebViewErrorSourceKey] integerValue]);
}
// Utility function for converting the WKWebViewErrorSource to the NSError
// received by WKWebViews.
NSError* WKWebViewErrorWithSource(NSError* error, WKWebViewErrorSource source) {
  DCHECK(error);
  base::scoped_nsobject<NSMutableDictionary> userInfo(
      [error.userInfo mutableCopy]);
  [userInfo setObject:@(source) forKey:kWKWebViewErrorSourceKey];
  return
      [NSError errorWithDomain:error.domain code:error.code userInfo:userInfo];
}
}  // namespace

#pragma mark -

// A container object for any navigation information that is only available
// during pre-commit delegate callbacks, and thus must be held until the
// navigation commits and the informatino can be used.
@interface CRWWebControllerPendingNavigationInfo : NSObject {
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_CRWWebControllerPendingNavigationInfo;
}
// The referrer for the page.
@property(nonatomic, copy) NSString* referrer;
// The MIME type for the page.
@property(nonatomic, copy) NSString* MIMEType;
// The navigation type for the load.
@property(nonatomic, assign) WKNavigationType navigationType;
// HTTP request method for the load.
@property(nonatomic, copy) NSString* HTTPMethod;
// Whether the pending navigation has been directly cancelled before the
// navigation is committed.
// Cancelled navigations should be simply discarded without handling any
// specific error.
@property(nonatomic, assign) BOOL cancelled;
@end

@implementation CRWWebControllerPendingNavigationInfo
@synthesize referrer = _referrer;
@synthesize MIMEType = _MIMEType;
@synthesize navigationType = _navigationType;
@synthesize HTTPMethod = _HTTPMethod;
@synthesize cancelled = _cancelled;

- (instancetype)init {
  if ((self = [super init])) {
    _propertyReleaser_CRWWebControllerPendingNavigationInfo.Init(
        self, [CRWWebControllerPendingNavigationInfo class]);
    _navigationType = WKNavigationTypeOther;
  }
  return self;
}
@end

@interface CRWWebController ()<CRWNativeContentDelegate,
                               CRWSSLStatusUpdaterDataSource,
                               CRWSSLStatusUpdaterDelegate,
                               CRWWebControllerContainerViewDelegate,
                               CRWWebViewScrollViewProxyObserver,
                               WKNavigationDelegate,
                               WKUIDelegate> {
  base::WeakNSProtocol<id<CRWWebDelegate>> _delegate;
  base::WeakNSProtocol<id<CRWWebUserInterfaceDelegate>> _UIDelegate;
  base::WeakNSProtocol<id<CRWNativeContentProvider>> _nativeProvider;
  base::WeakNSProtocol<id<CRWSwipeRecognizerProvider>> _swipeRecognizerProvider;
  // The WKWebView managed by this instance.
  base::scoped_nsobject<WKWebView> _webView;
  // The CRWWebViewProxy is the wrapper to give components access to the
  // web view in a controlled and limited way.
  base::scoped_nsobject<CRWWebViewProxyImpl> _webViewProxy;
  // The view used to display content.  Must outlive |_webViewProxy|. The
  // container view should be accessed through this property rather than
  // |self.view| from within this class, as |self.view| triggers creation while
  // |self.containerView| will return nil if the view hasn't been instantiated.
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
  // YES if a user interaction has been registered at any time once the page has
  // loaded.
  BOOL _userInteractionRegistered;
  // YES if the user has interacted with the content area since the last URL
  // change.
  BOOL _interactionRegisteredSinceLastURLChange;
  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/549616): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative.
  GURL _documentURL;
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
  // Can be nil if has not been calculated yet. Precalculation is necessary
  // because retreiving DOM element relies on async API so element info can not
  // be built on demand. May contain the following keys: @"href", @"src",
  // @"title", @"referrerPolicy". All values are strings. Used for showing
  // context menus.
  base::scoped_nsobject<NSDictionary> _DOMElementForLastTouch;
  // Whether a click is in progress.
  BOOL _clickInProgress;
  // Data on the recorded last user interaction.
  std::unique_ptr<web::UserInteractionEvent> _lastUserInteraction;
  // YES if there has been user interaction with views owned by this controller.
  BOOL _userInteractedWithWebController;
  // The time of the last page transfer start, measured in seconds since Jan 1
  // 2001.
  CFAbsoluteTime _lastTransferTimeInSeconds;
  // Default URL (about:blank).
  GURL _defaultURL;
  // Show overlay view, don't reload web page.
  BOOL _overlayPreviewMode;
  // If |YES|, calls |setShouldSuppressDialogs:YES| when window id is injected
  // into the web view.
  BOOL _shouldSuppressDialogsOnWindowIDInjection;
  // The URL of an expected future recreation of the |webView|. Valid
  // only if the web view was discarded for non-user-visible reasons, such that
  // if the next load request is for that URL, it should be treated as a
  // reconstruction that should use cache aggressively.
  GURL _expectedReconstructionURL;
  // Whether the web page is currently performing window.history.pushState or
  // window.history.replaceState
  // Set to YES on window.history.willChangeState message. To NO on
  // window.history.didPushState or window.history.didReplaceState.
  BOOL _changingHistoryState;
  // Set to YES when a hashchange event is manually dispatched for same-document
  // history navigations.
  BOOL _dispatchingSameDocumentHashChangeEvent;
  // YES if the web process backing _wkWebView is believed to currently be dead.
  BOOL _webProcessIsDead;

  std::unique_ptr<web::NewWindowInfo> _externalRequest;
  // Object for loading POST requests with body.
  base::scoped_nsobject<CRWJSPOSTRequestLoader> _POSTRequestLoader;

  // WebStateImpl instance associated with this CRWWebController, web controller
  // does not own this pointer.
  WebStateImpl* _webStateImpl;

  // A set of URLs opened in external applications; stored so that errors
  // from the web view can be identified as resulting from these events.
  base::scoped_nsobject<NSMutableSet> _openedApplicationURL;

  // A set of script managers whose scripts have been injected into the current
  // page.
  // TODO(stuartmorgan): Revisit this approach; it's intended only as a stopgap
  // measure to make all the existing script managers work. Longer term, there
  // should probably be a couple of points where managers can register to have
  // things happen automatically based on page lifecycle, and if they don't want
  // to use one of those fixed points, they should make their scripts internally
  // idempotent.
  base::scoped_nsobject<NSMutableSet> _injectedScriptManagers;

  // Script manager for setting the windowID.
  base::scoped_nsobject<CRWJSWindowIDManager> _windowIDJSManager;

  // The receiver of JavaScripts.
  base::scoped_nsobject<CRWJSInjectionReceiver> _jsInjectionReceiver;

  // Handles downloading PassKit data for WKWebView. Lazy initialized.
  base::scoped_nsobject<CRWPassKitDownloader> _passKitDownloader;

  // Backs up property with the same name.
  std::unique_ptr<web::MojoFacade> _mojoFacade;

  // Referrer for the current page; does not include the fragment.
  base::scoped_nsobject<NSString> _currentReferrerString;

  // Pending information for an in-progress page navigation. The lifetime of
  // this object starts at |decidePolicyForNavigationAction| where the info is
  // extracted from the request, and ends at either |didCommitNavigation| or
  // |didFailProvisionalNavigation|.
  base::scoped_nsobject<CRWWebControllerPendingNavigationInfo>
      _pendingNavigationInfo;

  // The WKNavigation for the most recent load request.
  base::scoped_nsobject<WKNavigation> _latestWKNavigation;

  // The WKNavigation captured when |stopLoading| was called. Used for reporting
  // WebController.EmptyNavigationManagerCausedByStopLoading UMA metric which
  // helps with diagnosing a navigation related crash (crbug.com/565457).
  base::WeakNSObject<WKNavigation> _stoppedWKNavigation;

  // CRWWebUIManager object for loading WebUI pages.
  base::scoped_nsobject<CRWWebUIManager> _webUIManager;

  // Updates SSLStatus for current navigation item.
  base::scoped_nsobject<CRWSSLStatusUpdater> _SSLStatusUpdater;

  // Controller used for certs verification to help with blocking requests with
  // bad SSL cert, presenting SSL interstitials and determining SSL status for
  // Navigation Items.
  base::scoped_nsobject<CRWCertVerificationController>
      _certVerificationController;

  // CertVerification errors which happened inside
  // |webView:didReceiveAuthenticationChallenge:completionHandler:|.
  // Key is leaf-cert/host pair. This storage is used to carry calculated
  // cert status from |didReceiveAuthenticationChallenge:| to
  // |didFailProvisionalNavigation:| delegate method.
  std::unique_ptr<CertVerificationErrorsCacheType> _certVerificationErrors;
}

// If |contentView_| contains a web view, this is the web view it contains.
// If not, it's nil.
@property(nonatomic, readonly) WKWebView* webView;
// The scroll view of |webView|.
@property(nonatomic, readonly) UIScrollView* webScrollView;
// The current page state of the web view. Writing to this property
// asynchronously applies the passed value to the current web view.
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
// The currently displayed native controller, if any.
@property(nonatomic, readwrite) id<CRWNativeContent> nativeController;
// Returns NavigationManager's session controller.
@property(nonatomic, readonly) CRWSessionController* sessionController;
// Activity indicator group ID for this web controller.
@property(nonatomic, readonly) NSString* activityIndicatorGroupID;
// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(nonatomic, readonly) NSDictionary* WKWebViewObservers;
// Downloader for PassKit files. Lazy initialized.
@property(nonatomic, readonly) CRWPassKitDownloader* passKitDownloader;

// The web view's view of the current URL. During page transitions
// this may not be the same as the session history's view of the current URL.
// This method can change the state of the CRWWebController, as it will display
// an error if the returned URL is not reliable from a security point of view.
// Note that this method is expensive, so it should always be cached locally if
// it's needed multiple times in a method.
@property(nonatomic, readonly) GURL currentURL;
// Returns the referrer for the current page.
@property(nonatomic, readonly) web::Referrer currentReferrer;

// Returns YES if the user interacted with the page recently.
@property(nonatomic, readonly) BOOL userClickedRecently;
// Returns whether the desktop user agent should be used when setting the user
// agent.
@property(nonatomic, readonly) BOOL useDesktopUserAgent;

// Facade for Mojo API.
@property(nonatomic, readonly) web::MojoFacade* mojoFacade;

// Removes the container view from the hierarchy and resets the ivar.
- (void)resetContainerView;
// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported to the delegate, and internal state updated to
// reflect the fact that the navigation has occurred.
// TODO(stuartmorgan): The code conflates URL changes and document object
// changes; the two need to be separated and handled differently.
- (void)webPageChanged;
// Resets any state that is associated with a specific document object (e.g.,
// page interaction tracking).
- (void)resetDocumentSpecificState;
// Called when a page (native or web) has actually started loading (i.e., for
// a web page the document has actually changed), or after the load request has
// been registered for a non-document-changing URL change. Updates internal
// state not specific to web pages, and informs the delegate.
- (void)didStartLoadingURL:(const GURL&)URL updateHistory:(BOOL)updateHistory;
// Returns YES if the URL looks like it is one CRWWebController can show.
+ (BOOL)webControllerCanShow:(const GURL&)url;
// Clears the currently-displayed transient content view.
- (void)clearTransientContentView;
// Returns a lazily created CRWTouchTrackingRecognizer.
- (CRWTouchTrackingRecognizer*)touchTrackingRecognizer;
// Adds an activity indicator tasks for this web controller.
- (void)addActivityIndicatorTask;
// Clears all activity indicator tasks for this web controller.
- (void)clearActivityIndicatorTasks;
// Shows placeholder overlay.
- (void)addPlaceholderOverlay;
// Removes placeholder overlay.
- (void)removePlaceholderOverlay;

// Returns the current entry from the underlying session controller.
// TODO(stuartmorgan): Audit all calls to these methods; these are just wrappers
// around the same logic as GetActiveEntry, so should probably not be used for
// the same reason that GetActiveEntry is deprecated. (E.g., page operations
// should generally be dealing with the last commited entry, not a pending
// entry).
- (CRWSessionEntry*)currentSessionEntry;
// Returns the navigation item for the current page.
- (web::NavigationItem*)currentNavItem;
// Returns the URL that the navigation system believes should be currently
// active.
// TODO(stuartmorgan):Remove this in favor of more specific getters.
- (const GURL&)currentNavigationURL;
// Returns the current transition type.
- (ui::PageTransition)currentTransition;
// Returns the referrer for current navigation item. May be empty.
- (web::Referrer)currentSessionEntryReferrer;
// The HTTP headers associated with the current navigation item. These are nil
// unless the request was a POST.
- (NSDictionary*)currentHTTPHeaders;

// Creates a web view if it's not yet created.
- (void)ensureWebViewCreated;
// Creates a web view with given |config|. No-op if web view is already created.
- (void)ensureWebViewCreatedWithConfiguration:(WKWebViewConfiguration*)config;
// Returns a new autoreleased web view created with given configuration.
- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config;
// Sets the value of the webView property, and performs its basic setup.
- (void)setWebView:(WKWebView*)webView;
// Removes webView, optionally tracking the URL of the evicted
// page for later cache-based reconstruction.
- (void)removeWebViewAllowingCachedReconstruction:(BOOL)allowCache;
// Called when web view process has been terminated.
- (void)webViewWebProcessDidCrash;
// Returns the WKWebViewConfigurationProvider associated with the web
// controller's BrowserState.
- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider;
// Returns the type of document object loaded in the web view.
- (web::WebViewDocumentType)webViewDocumentType;
// Converts MIME type string to WebViewDocumentType.
- (web::WebViewDocumentType)documentTypeFromMIMEType:(NSString*)MIMEType;
// Extracts Referer value from WKNavigationAction request header.
- (NSString*)refererFromNavigationAction:(WKNavigationAction*)action;

// Returns the current URL of the web view, and sets |trustLevel| accordingly
// based on the confidence in the verification.
- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel;
// Returns |YES| if |url| should be loaded in a native view.
- (BOOL)shouldLoadURLInNativeView:(const GURL&)url;
// Loads the request into the |webView|.
- (void)loadRequest:(NSMutableURLRequest*)request;
// Loads POST request with body in |_wkWebView| by constructing an HTML page
// that executes the request through JavaScript and replaces document with the
// result.
// Note that this approach includes multiple body encodings and decodings, plus
// the data is passed to |_wkWebView| on main thread.
// This is necessary because WKWebView ignores POST request body.
// Workaround for https://bugs.webkit.org/show_bug.cgi?id=145410
- (void)loadPOSTRequest:(NSMutableURLRequest*)request;
// Loads the HTML into the page at the given URL.
- (void)loadHTML:(NSString*)html forURL:(const GURL&)url;

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationAction|, but must be in a pending state until
// |didgo/Navigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action;
// Extracts navigation info from WKNavigationResponse and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationResponse|, but must be in a pending state until
// |didCommitNavigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationResponse:
    (WKNavigationResponse*)response;
// Updates current state with any pending information. Should be called when a
// navigation is committed.
- (void)commitPendingNavigationInfo;
// Returns a NSMutableURLRequest that represents the current NavigationItem.
- (NSMutableURLRequest*)requestForCurrentNavigationItem;
// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder;
// Updates the WKBackForwardListItemHolder navigation item.
- (void)updateCurrentBackForwardListItemHolder;

// Loads the current nativeController in a native view. If a web view is
// present, removes it and swaps in the native view in its place.
- (void)loadNativeViewWithSuccess:(BOOL)loadSuccess;
// YES if the navigation to |url| should be treated as a reload.
- (BOOL)shouldReload:(const GURL&)destinationURL
          transition:(ui::PageTransition)transition;
// Internal implementation of reload. Reloads without notifying the delegate.
// Most callers should use -reload instead.
- (void)reloadInternal;
// Aborts any load for both the web view and web controller.
- (void)abortLoad;
// Updates the internal state and informs the delegate that any outstanding load
// operations are cancelled.
- (void)loadCancelled;
// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad;
// Called following navigation completion to generate final navigation lifecycle
// events. Navigation is considered complete when the document has finished
// loading, or when other page load mechanics are completed on a
// non-document-changing URL change.
- (void)didFinishNavigation;
// Update the appropriate parts of the model and broadcast to the embedder. This
// may be called multiple times and thus must be idempotent.
- (void)loadCompleteWithSuccess:(BOOL)loadSuccess;
// Called after URL is finished loading and _loadPhase is set to PAGE_LOADED.
- (void)didFinishWithURL:(const GURL&)currentURL loadSuccess:(BOOL)loadSuccess;
// Navigates forwards or backwards by |delta| pages. No-op if delta is out of
// bounds. Reloads if delta is 0.
// TODO(crbug.com/661316): Move this method to NavigationManager.
- (void)goDelta:(int)delta;
// Loads a new URL if the current entry is not from a pushState() navigation.
// |fromEntry| is the CRWSessionEntry that was the current entry prior to the
// navigation.
- (void)finishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry;
// Informs the native controller if web usage is allowed or not.
- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled;
// Called when web controller receives a new message from the web page.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message;
// Returns a new script which wraps |script| with windowID check so |script| is
// not evaluated on windowID mismatch.
- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script;
// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;
// Registers load request with empty referrer and link or client redirect
// transition based on user interaction state.
- (void)registerLoadRequest:(const GURL&)URL;
// Prepares web controller and delegates for anticipated page change.
// Allows several methods to invoke webWill/DidAddPendingURL on anticipated page
// change, using the same cached request and calculated transition types.
- (void)registerLoadRequest:(const GURL&)URL
                   referrer:(const web::Referrer&)referrer
                 transition:(ui::PageTransition)transition;
// Updates the HTML5 history state of the page using the current NavigationItem.
// For same-document navigations and navigations affected by
// window.history.[push/replace]State(), the URL and serialized state object
// will be updated to the current NavigationItem's values.  A popState event
// will be triggered for all same-document navigations.  Additionaly, a
// hashchange event will be triggered for same-document navigations where the
// only difference between the current and previous URL is the fragment.
- (void)updateHTML5HistoryState;
// Generates the JavaScript string used to update the UIWebView's URL so that it
// matches the URL displayed in the omnibox and sets window.history.state to
// stateObject. Needed for history.pushState() and history.replaceState().
- (NSString*)javaScriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject;
// Generates the JavaScript string used to manually dispatch a popstate event,
// using |stateObjectJSON| as the event parameter.
- (NSString*)javaScriptToDispatchPopStateWithObject:(NSString*)stateObjectJSON;
// Generates the JavaScript string used to manually dispatch a hashchange event,
// using |oldURL| and |newURL| as the event parameters.
- (NSString*)javaScriptToDispatchHashChangeWithOldURL:(const GURL&)oldURL
                                               newURL:(const GURL&)newURL;
// Injects JavaScript to update the URL and state object of the webview to the
// values found in the current NavigationItem.  A hashchange event will be
// dispatched if |dispatchHashChange| is YES, and a popstate event will be
// dispatched if |sameDocument| is YES.  Upon the script's completion, resets
// |urlOnStartLoading_| and |_lastRegisteredRequestURL| to the current
// NavigationItem's URL.  This is necessary so that sites that depend on URL
// params/fragments continue to work correctly and that checks for the URL don't
// incorrectly trigger |-webPageChanged| calls.
- (void)injectHTML5HistoryScriptWithHashChange:(BOOL)dispatchHashChange
                        sameDocumentNavigation:(BOOL)sameDocumentNavigation;

- (BOOL)isLoaded;
// Extracts the current page's viewport tag information and calls |completion|.
// If the page has changed before the viewport tag is successfully extracted,
// |completion| is called with nullptr.
typedef void (^ViewportStateCompletion)(const web::PageViewportState*);
- (void)extractViewportTagWithCompletion:(ViewportStateCompletion)completion;
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
// Sets zoom scale value for webview scroll view from |zoomState|.
- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState;
// Sets scroll offset value for webview scroll view from |scrollState|.
- (void)applyWebViewScrollOffsetFromScrollState:
    (const web::PageScrollState&)scrollState;
// Asynchronously fetches full width of the rendered web page.
- (void)fetchWebPageWidthWithCompletionHandler:(void (^)(CGFloat))handler;
// Asynchronously fetches information about DOM element for the given point (in
// UIView coordinates). |handler| can not be nil. See |_DOMElementForLastTouch|
// for element format description.
- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler;
// Extracts context menu information from the given DOM element.
- (web::ContextMenuParams)contextMenuParamsForElement:(NSDictionary*)element;
// Sets the value of |_DOMElementForLastTouch|.
- (void)setDOMElementForLastTouch:(NSDictionary*)element;
// Called when the window has determined there was a long-press and context menu
// must be shown.
- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer;
// Cancels all touch events in the web view (long presses, tapping, scrolling).
- (void)cancelAllTouches;
// Returns the referrer for the current page.
- (web::Referrer)currentReferrer;
// Asynchronously returns the referrer policy for the current page.
- (void)queryPageReferrerPolicy:(void (^)(NSString*))responseHandler;
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
// Sets _documentURL to newURL, and updates any relevant state information.
- (void)setDocumentURL:(const GURL&)newURL;
// Returns YES if the current navigation item corresponds to a web page
// loaded by a POST request.
- (BOOL)isCurrentNavigationItemPOST;
// Returns YES if current navigation item is WKNavigationTypeBackForward.
- (BOOL)isCurrentNavigationBackForward;
// Returns whether the given navigation is triggered by a user link click.
- (BOOL)isLinkNavigation:(WKNavigationType)navigationType;

// Inject windowID if not yet injected.
- (void)injectWindowID;
// Creates a new opened by DOM window and returns its autoreleased web
// controller.
- (CRWWebController*)createChildWebController;

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;
// Compares the two URLs being navigated between during a history navigation to
// determine if a # needs to be appended to the URL of |toItem| to trigger a
// hashchange event. If so, also saves the modified URL into |toItem|.
- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem;

// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)optOutScrollsToTopForSubviews;
// Tears down the old native controller, and then replaces it with the new one.
- (void)setNativeController:(id<CRWNativeContent>)nativeController;
// Returns whether |url| should be opened.
- (BOOL)shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked;
// Called when |URL| needs to be opened in a matching native app.
// Returns YES if the url was succesfully opened in the native app.
- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)URL
                         sourceURL:(const GURL&)sourceURL
           linkActivatedNavigation:(BOOL)linkActivatedNavigation;
// Called when a JavaScript dialog, HTTP authentication dialog or window.open
// call has been suppressed.
- (void)didSuppressDialog;
// Convenience method to inform CWRWebDelegate about a blocked popup.
- (void)didBlockPopupWithURL:(GURL)popupURL sourceURL:(GURL)sourceURL;
// Informs CWRWebDelegate that CRWWebController has detected and blocked a
// popup.
- (void)didBlockPopupWithURL:(GURL)popupURL
                   sourceURL:(GURL)sourceURL
              referrerPolicy:(const std::string&)referrerPolicyString;
// Returns YES if the navigation action is associated with a main frame request.
- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action;
// Returns whether external URL navigation action should be opened.
- (BOOL)shouldOpenExternalURLForNavigationAction:(WKNavigationAction*)action;
// Called when a page updates its history stack using pushState or replaceState.
- (void)didUpdateHistoryStateWithPageURL:(const GURL&)url;
// Updates SSL status for the current navigation item based on the information
// provided by web view.
- (void)updateSSLStatusForCurrentNavigationItem;
// Called when SSL status has been updated for the current navigation item.
- (void)didUpdateSSLStatusForCurrentNavigationItem;
// Called when a load ends in an SSL error and certificate chain.
- (void)handleSSLCertError:(NSError*)error;

// Returns YES if the popup should be blocked, NO otherwise.
- (BOOL)shouldBlockPopupWithURL:(const GURL&)popupURL
                      sourceURL:(const GURL&)sourceURL;
// Tries to open a popup with the given new window information.
- (void)openPopupWithInfo:(const web::NewWindowInfo&)windowInfo;

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to
// reply with NSURLSessionAuthChallengeDisposition and credentials.
- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler;
// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler;
// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
+ (void)processHTTPAuthForUser:(NSString*)user
                      password:(NSString*)password
             completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                         NSURLCredential*))completionHandler;

// Helper to respond to |webView:runJavaScript...| delegate methods.
// |completionHandler| must not be nil.
- (void)runJavaScriptDialogOfType:(web::JavaScriptDialogType)type
                 initiatedByFrame:(WKFrameInfo*)frame
                          message:(NSString*)message
                      defaultText:(NSString*)defaultText
                       completion:(void (^)(BOOL, NSString*))completionHandler;

// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange;
// Called when WKWebView certificateChain or hasOnlySecureContent property has
// changed.
- (void)webViewSecurityFeaturesDidChange;
// Called when WKWebView loading state has been changed.
- (void)webViewLoadingStateDidChange;
// Called when WKWebView title has been changed.
- (void)webViewTitleDidChange;
// Called when WKWebView URL has been changed.
- (void)webViewURLDidChange;
// Returns YES if a KVO change to |newURL| could be a 'navigation' within the
// document (hash change, pushState/replaceState, etc.). This should only be
// used in the context of a URL KVO callback firing, and only if |isLoading| is
// YES for the web view (since if it's not, no guesswork is needed).
- (BOOL)isKVOChangePotentialSameDocumentNavigationToURL:(const GURL&)newURL;
// Called when a non-document-changing URL change occurs. Updates the
// _documentURL, and informs the superclass of the change.
- (void)URLDidChangeWithoutDocumentChange:(const GURL&)URL;
// Returns YES if there is currently a requested but uncommitted load for
// |targetURL|.
- (BOOL)isLoadRequestPendingForURL:(const GURL&)targetURL;
// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call |loadRequest| on the underlying
// web view, or use native web view navigation where possible (for example,
// going back and forward through the history stack).
- (void)loadRequestForCurrentNavigationItem;

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
// Handles 'geolocationDialog.suppressed' message.
- (BOOL)handleGeolocationDialogSuppressedMessage:(base::DictionaryValue*)message
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
// Handles 'window.history.willChangeState' message.
- (BOOL)handleWindowHistoryWillChangeStateMessage:
            (base::DictionaryValue*)message
                                          context:(NSDictionary*)context;
// Handles 'window.history.didPushState' message.
- (BOOL)handleWindowHistoryDidPushStateMessage:(base::DictionaryValue*)message
                                       context:(NSDictionary*)context;

// Handles 'window.history.didReplaceState' message.
- (BOOL)handleWindowHistoryDidReplaceStateMessage:
            (base::DictionaryValue*)message
                                          context:(NSDictionary*)context;

// Returns YES if the given |action| should be allowed to continue.
// If this returns NO, the load should be cancelled.
- (BOOL)shouldAllowLoadWithNavigationAction:(WKNavigationAction*)action;
// Called when a load ends in an error.
// TODO(stuartmorgan): Figure out if there's actually enough shared logic that
// this makes sense. At the very least remove inMainFrame since that only makes
// sense for UIWebView.
- (void)handleLoadError:(NSError*)error inMainFrame:(BOOL)inMainFrame;

// Handles cancelled load in WKWebView (error with NSURLErrorCancelled code).
- (void)handleCancelledError:(NSError*)error;

// Used to decide whether a load that generates errors with the
// NSURLErrorCancelled code should be cancelled.
- (BOOL)shouldCancelLoadForCancelledError:(NSError*)error;

// Sets up WebUI for URL.
- (void)createWebUIForURL:(const GURL&)URL;
// Clears WebUI, if one exists.
- (void)clearWebUI;

@end

namespace {

NSString* const kReferrerHeaderName = @"Referer";  // [sic]

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

// Leave snapshot overlay up unless page loads.
const NSTimeInterval kSnapshotOverlayDelay = 1.5;
// Transition to fade snapshot overlay.
const NSTimeInterval kSnapshotOverlayTransition = 0.5;

}  // namespace

@implementation CRWWebController

@synthesize webUsageEnabled = _webUsageEnabled;
@synthesize usePlaceholderOverlay = _usePlaceholderOverlay;
@synthesize loadPhase = _loadPhase;
@synthesize shouldSuppressDialogs = _shouldSuppressDialogs;

- (instancetype)initWithWebState:(WebStateImpl*)webState {
  self = [super init];
  if (self) {
    _webStateImpl = webState;
    DCHECK(_webStateImpl);
    _webStateImpl->InitializeRequestTracker(self);
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _loadPhase = web::PAGE_LOADED;
    // Content area is lazily instantiated.
    _defaultURL = GURL(url::kAboutBlankURL);
    _jsInjectionReceiver.reset(
        [[CRWJSInjectionReceiver alloc] initWithEvaluator:self]);
    _webViewProxy.reset(
        [[CRWWebViewProxyImpl alloc] initWithWebController:self]);
    [[_webViewProxy scrollViewProxy] addObserver:self];
    _gestureRecognizers.reset([[NSMutableArray alloc] init]);
    _webViewToolbars.reset([[NSMutableArray alloc] init]);
    _pendingLoadCompleteActions.reset([[NSMutableArray alloc] init]);
    web::BrowserState* browserState = _webStateImpl->GetBrowserState();
    _certVerificationController.reset([[CRWCertVerificationController alloc]
        initWithBrowserState:browserState]);
    _certVerificationErrors.reset(
        new CertVerificationErrorsCacheType(kMaxCertErrorsCount));
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
  return _webStateImpl;
}

- (WebStateImpl*)webStateImpl {
  return _webStateImpl;
}

- (void)clearTransientContentView {
  // Early return if there is no transient content view.
  if (![_containerView transientContentView])
    return;

  // Remove the transient content view from the hierarchy.
  [_containerView clearTransientContentView];

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
  [_containerView displayTransientContent:contentView];
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

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
  DCHECK(_isBeingDestroyed);  // 'close' must have been called already.
  DCHECK(!_webView);
  // TODO(crbug.com/662860): Don't set the delegate to nil.
  [_containerView setDelegate:nil];
  _touchTrackingRecognizer.get().touchTrackingDelegate = nil;
  [[_webViewProxy scrollViewProxy] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)dismissKeyboard {
  [_webView endEditing:YES];
  if ([self.nativeController respondsToSelector:@selector(dismissKeyboard)])
    [self.nativeController dismissKeyboard];
}

- (id<CRWNativeContent>)nativeController {
  return [_containerView nativeController];
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  // Check for pointer equality.
  if (self.nativeController == nativeController)
    return;

  // Unset the delegate on the previous instance.
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)])
    [self.nativeController setDelegate:nil];

  [_containerView displayNativeContent:nativeController];
  [self setNativeControllerWebUsageEnabled:_webUsageEnabled];
}

- (NSString*)activityIndicatorGroupID {
  return [NSString
      stringWithFormat:@"WebController.NetworkActivityIndicatorKey.%@",
                       self.webStateImpl->GetRequestGroupID()];
}

- (NSDictionary*)WKWebViewObservers {
  return @{
    @"certificateChain" : @"webViewSecurityFeaturesDidChange",
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"hasOnlySecureContent" : @"webViewSecurityFeaturesDidChange",
    @"loading" : @"webViewLoadingStateDidChange",
    @"title" : @"webViewTitleDidChange",
    @"URL" : @"webViewURLDidChange",
  };
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
  [_containerView removeFromSuperview];
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
  if (_webView) {
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

- (BOOL)isViewAlive {
  return !_webProcessIsDead && [_containerView isViewAlive];
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
  _webStateImpl->CancelDialogs();

  _SSLStatusUpdater.reset();

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

  _webStateImpl = nullptr;
}

- (void)setDOMElementForLastTouch:(NSDictionary*)element {
  _DOMElementForLastTouch.reset([element copy]);
}

- (void)showContextMenu:(UIGestureRecognizer*)gestureRecognizer {
  // We don't want ongoing notification that the long press is held.
  if ([gestureRecognizer state] != UIGestureRecognizerStateBegan)
    return;

  if (![_DOMElementForLastTouch count])
    return;

  web::ContextMenuParams params =
      [self contextMenuParamsForElement:_DOMElementForLastTouch.get()];
  params.view.reset([_webView retain]);
  params.location = [gestureRecognizer locationInView:_webView];
  if (self.webStateImpl->HandleContextMenu(params)) {
    // Cancelling all touches has the intended side effect of suppressing the
    // system's context menu.
    [self cancelAllTouches];
  }
}

- (void)cancelAllTouches {
  // Disable web view scrolling.
  CancelTouches(self.webScrollView.panGestureRecognizer);

  // All user gestures are handled by a subview of web view scroll view
  // (WKContentView).
  for (UIView* subview in self.webScrollView.subviews) {
    for (UIGestureRecognizer* recognizer in subview.gestureRecognizers) {
      CancelTouches(recognizer);
    }
  }

  // Just disabling/enabling the gesture recognizers is not enough to suppress
  // the click handlers on the JS side. This JS performs the function of
  // suppressing these handlers on the JS side.
  NSString* suppressNextClick = @"__gCrWeb.suppressNextClick()";
  [self executeJavaScript:suppressNextClick completionHandler:nil];
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

- (CGPoint)scrollPosition {
  CGPoint position = CGPointMake(0.0, 0.0);
  if (!self.webScrollView)
    return position;
  return self.webScrollView.contentOffset;
}

- (BOOL)atTop {
  if (!_webView)
    return YES;
  UIScrollView* scrollView = self.webScrollView;
  return scrollView.contentOffset.y == -scrollView.contentInset.top;
}

- (void)setShouldSuppressDialogs:(BOOL)shouldSuppressDialogs {
  _shouldSuppressDialogs = shouldSuppressDialogs;
  if (_webView) {
    NSString* const kSetSuppressDialogs = [NSString
        stringWithFormat:@"__gCrWeb.setSuppressGeolocationDialogs(%d);",
                         shouldSuppressDialogs];
    [self executeJavaScript:kSetSuppressDialogs completionHandler:nil];
    _shouldSuppressDialogsOnWindowIDInjection = NO;
  } else {
    _shouldSuppressDialogsOnWindowIDInjection = shouldSuppressDialogs;
  }
}

- (GURL)currentURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel) << "Verification of the trustLevel state is mandatory";
  if (_webView) {
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

- (WKWebView*)webView {
  return _webView.get();
}

- (UIScrollView*)webScrollView {
  return [_webView scrollView];
}

- (GURL)currentURL {
  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  return [self currentURLWithTrustLevel:&trustLevel];
}

- (web::Referrer)currentReferrer {
  // Referrer string doesn't include the fragment, so in cases where the
  // previous URL is equal to the current referrer plus the fragment the
  // previous URL is returned as current referrer.
  NSString* referrerString = _currentReferrerString;

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

- (void)queryPageReferrerPolicy:(void (^)(NSString*))responseHandler {
  DCHECK(responseHandler);
  [self executeJavaScript:@"__gCrWeb.getPageReferrerPolicy()"
        completionHandler:^(id referrer, NSError* error) {
          DCHECK_NE(error.code, WKErrorJavaScriptExceptionOccurred);
          responseHandler(base::mac::ObjCCast<NSString>(referrer));
        }];
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

- (void)setDocumentURL:(const GURL&)newURL {
  if (newURL != _documentURL && newURL.is_valid()) {
    _documentURL = newURL;
    _interactionRegisteredSinceLastURLChange = NO;
  }
}

- (BOOL)isCurrentNavigationItemPOST {
  // |_pendingNavigationInfo| will be nil if the decidePolicy* delegate methods
  // were not called.
  NSString* HTTPMethod =
      _pendingNavigationInfo
          ? [_pendingNavigationInfo HTTPMethod]
          : [self currentBackForwardListItemHolder]->http_method();
  return [HTTPMethod isEqual:@"POST"];
}

- (BOOL)isCurrentNavigationBackForward {
  if (![self currentNavItem])
    return NO;
  WKNavigationType currentNavigationType =
      [self currentBackForwardListItemHolder]->navigation_type();
  return currentNavigationType == WKNavigationTypeBackForward;
}

- (BOOL)isLinkNavigation:(WKNavigationType)navigationType {
  switch (navigationType) {
    case WKNavigationTypeLinkActivated:
      return YES;
    case WKNavigationTypeOther:
      // Sometimes link navigation is not detected by navigation type, so
      // check last user interaction with the page in the recent time.
      return [self userClickedRecently];
    default:
      return NO;
  }
}

- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = [_webView backForwardList];
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem {
  // If navigating with native API, i.e. using a back forward list item,
  // hashchange events will be triggered automatically, so no URL tampering is
  // required.
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(toItem);
  if (holder->back_forward_list_item()) {
    return toItem->GetURL();
  }

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

- (void)injectWindowID {
  // Default value for shouldSuppressDialogs is NO, so updating them only
  // when necessary is a good optimization.
  if (_shouldSuppressDialogsOnWindowIDInjection) {
    self.shouldSuppressDialogs = YES;
    _shouldSuppressDialogsOnWindowIDInjection = NO;
  }

  [_windowIDJSManager inject];
}

// Set the specified recognizer to take priority over any recognizers in the
// view that have a description containing the specified text fragment.
+ (void)requireGestureRecognizerToFail:(UIGestureRecognizer*)recognizer
                                inView:(UIView*)view
                 containingDescription:(NSString*)fragment {
  for (UIGestureRecognizer* iRecognizer in [view gestureRecognizers]) {
    if (iRecognizer != recognizer) {
      NSString* description = [iRecognizer description];
      if ([description rangeOfString:fragment].length) {
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

- (CRWWebController*)createChildWebController {
  CRWWebController* result = [self.delegate webPageOrderedOpen];
  DCHECK(!result || result.sessionController.openedByDOM);
  return result;
}

- (BOOL)canUseViewForGeneratingOverlayPlaceholderView {
  return _containerView != nil;
}

- (UIView*)view {
  // Kick off the process of lazily creating the view and starting the load if
  // necessary; this creates _containerView if it doesn't exist.
  [self triggerPendingLoad];
  DCHECK(_containerView);
  return _containerView;
}

- (id<CRWWebViewProxy>)webViewProxy {
  return _webViewProxy.get();
}

- (UIView*)viewForPrinting {
  // TODO(ios): crbug.com/227944. Printing is not supported for native
  // controllers.
  return _webView;
}

- (double)loadingProgress {
  return [_webView estimatedProgress];
}

- (void)registerLoadRequest:(const GURL&)URL {
  // Get the navigation type from the last main frame load request, and try to
  // map that to a PageTransition.
  WKNavigationType navigationType =
      _pendingNavigationInfo ? [_pendingNavigationInfo navigationType]
                             : WKNavigationTypeOther;
  ui::PageTransition transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  switch (navigationType) {
    case WKNavigationTypeLinkActivated:
      transition = ui::PAGE_TRANSITION_LINK;
      break;
    case WKNavigationTypeFormSubmitted:
    case WKNavigationTypeFormResubmitted:
      transition = ui::PAGE_TRANSITION_FORM_SUBMIT;
      break;
    case WKNavigationTypeBackForward:
      transition = ui::PAGE_TRANSITION_FORWARD_BACK;
      break;
    case WKNavigationTypeReload:
      transition = ui::PAGE_TRANSITION_RELOAD;
      break;
    case WKNavigationTypeOther:
      // The "Other" type covers a variety of very different cases, which may
      // or may not be the result of user actions. For now, guess based on
      // whether there's been an interaction since the last URL change.
      // TODO(crbug.com/549301): See if this heuristic can be improved.
      transition = _interactionRegisteredSinceLastURLChange
                       ? ui::PAGE_TRANSITION_LINK
                       : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
      break;
  }
  // The referrer is not known yet, and will be updated later.
  const web::Referrer emptyReferrer;
  [self registerLoadRequest:URL referrer:emptyReferrer transition:transition];
}

- (void)registerLoadRequest:(const GURL&)requestURL
                   referrer:(const web::Referrer&)referrer
                 transition:(ui::PageTransition)transition {
  // Transfer time is registered so that further transitions within the time
  // envelope are not also registered as links.
  _lastTransferTimeInSeconds = CFAbsoluteTimeGetCurrent();
  if (!(transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK)) {
    // Before changing phases, the delegate should be informed that any existing
    // request is being cancelled before completion.
    [self loadCancelled];
    DCHECK(_loadPhase == web::PAGE_LOADED);
  }

  _loadPhase = web::LOAD_REQUESTED;
  _lastRegisteredRequestURL = requestURL;

  if (!(transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK)) {
    // Record state of outgoing page.
    [self recordStateInHistory];
  }

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
  _webStateImpl->SetIsLoading(true);
  [_delegate webDidAddPendingURL];
  _webStateImpl->OnProvisionalNavigationStarted(requestURL);
}

- (void)updateHTML5HistoryState {
  web::NavigationItemImpl* currentItem =
      static_cast<web::NavigationItemImpl*>([self currentNavItem]);
  if (!currentItem)
    return;

  // Same-document navigations must trigger a popState event.
  CRWSessionController* sessionController = self.sessionController;
  BOOL sameDocumentNavigation = [sessionController
      isSameDocumentNavigationBetweenEntry:sessionController.currentEntry
                                  andEntry:sessionController.previousEntry];
  // WKWebView doesn't send hashchange events for same-document non-BFLI
  // navigations, so one must be dispatched manually for hash change same-
  // document navigations.
  web::NavigationItem* previousItem =
      self.sessionController.previousEntry.navigationItem;
  const GURL URL = currentItem->GetURL();
  const GURL oldURL = previousItem ? previousItem->GetURL() : GURL();
  BOOL shouldDispatchHashchange = sameDocumentNavigation && previousItem &&
                                  (web::GURLByRemovingRefFromGURL(URL) ==
                                   web::GURLByRemovingRefFromGURL(oldURL));
  // The URL and state object must be set for same-document navigations and
  // NavigationItems that were created or updated by calls to pushState() or
  // replaceState().
  BOOL shouldUpdateState = sameDocumentNavigation ||
                           currentItem->IsCreatedFromPushState() ||
                           currentItem->HasStateBeenReplaced();
  if (!shouldUpdateState)
    return;

  // TODO(stuartmorgan): Make CRWSessionController manage this internally (or
  // remove it; it's not clear this matches other platforms' behavior).
  _webStateImpl->GetNavigationManagerImpl().OnNavigationItemCommitted();
  // Record that a same-document hashchange event will be fired.  This flag will
  // be reset when resonding to the hashchange message.  Note that resetting the
  // flag in the completion block below is too early, as that block is called
  // before hashchange event listeners have a chance to fire.
  _dispatchingSameDocumentHashChangeEvent = shouldDispatchHashchange;
  // Inject the JavaScript to update the state on the browser side.
  [self injectHTML5HistoryScriptWithHashChange:shouldDispatchHashchange
                        sameDocumentNavigation:sameDocumentNavigation];
}

- (NSString*)javaScriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject {
  std::string outURL;
  base::EscapeJSONString(URL.spec(), true, &outURL);
  return
      [NSString stringWithFormat:@"__gCrWeb.replaceWebViewURL(%@, %@);",
                                 base::SysUTF8ToNSString(outURL), stateObject];
}

- (NSString*)javaScriptToDispatchPopStateWithObject:(NSString*)stateObjectJSON {
  std::string outState;
  base::EscapeJSONString(base::SysNSStringToUTF8(stateObjectJSON), true,
                         &outState);
  return [NSString stringWithFormat:@"__gCrWeb.dispatchPopstateEvent(%@);",
                                    base::SysUTF8ToNSString(outState)];
}

- (NSString*)javaScriptToDispatchHashChangeWithOldURL:(const GURL&)oldURL
                                               newURL:(const GURL&)newURL {
  return [NSString
      stringWithFormat:@"__gCrWeb.dispatchHashchangeEvent(\'%s\', \'%s\');",
                       oldURL.spec().c_str(), newURL.spec().c_str()];
}

- (void)injectHTML5HistoryScriptWithHashChange:(BOOL)dispatchHashChange
                        sameDocumentNavigation:(BOOL)sameDocumentNavigation {
  web::NavigationItemImpl* currentItem =
      static_cast<web::NavigationItemImpl*>([self currentNavItem]);
  if (!currentItem)
    return;

  const GURL URL = currentItem->GetURL();
  NSString* stateObject = currentItem->GetSerializedStateObject();
  NSMutableString* script = [NSMutableString
      stringWithString:[self javaScriptToReplaceWebViewURL:URL
                                           stateObjectJSON:stateObject]];
  if (sameDocumentNavigation) {
    [script
        appendString:[self javaScriptToDispatchPopStateWithObject:stateObject]];
  }
  if (dispatchHashChange) {
    web::NavigationItemImpl* previousItem =
        self.sessionController.previousEntry.navigationItemImpl;
    const GURL oldURL = previousItem ? previousItem->GetURL() : GURL();
    [script appendString:[self javaScriptToDispatchHashChangeWithOldURL:oldURL
                                                                 newURL:URL]];
  }
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self executeJavaScript:script
        completionHandler:^(id, NSError*) {
          if (!weakSelf || weakSelf.get()->_isBeingDestroyed)
            return;
          base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
          strongSelf.get()->_URLOnStartLoading = URL;
          strongSelf.get()->_lastRegisteredRequestURL = URL;
        }];
}

// Load the current URL in a web view, first ensuring the web view is visible.
- (void)loadCurrentURLInWebView {
  // Clear the set of URLs opened in external applications.
  _openedApplicationURL.reset([[NSMutableSet alloc] init]);

  // Load the url. The UIWebView delegate callbacks take care of updating the
  // session history and UI.
  const GURL targetURL([self currentNavigationURL]);
  if (!targetURL.is_valid()) {
    [self didFinishWithURL:targetURL loadSuccess:NO];
    return;
  }

  // JavaScript should never be evaluated here. User-entered JS should be
  // evaluated via stringByEvaluatingUserJavaScriptFromString.
  DCHECK(!targetURL.SchemeIs(url::kJavaScriptScheme));

  [self ensureWebViewCreated];

  [self loadRequestForCurrentNavigationItem];
}

- (void)updatePendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame) {
    _pendingNavigationInfo.reset(
        [[CRWWebControllerPendingNavigationInfo alloc] init]);
    [_pendingNavigationInfo
        setReferrer:[self refererFromNavigationAction:action]];
    [_pendingNavigationInfo setNavigationType:action.navigationType];
    [_pendingNavigationInfo setHTTPMethod:action.request.HTTPMethod];
  }
}

- (void)updatePendingNavigationInfoFromNavigationResponse:
    (WKNavigationResponse*)response {
  if (response.isForMainFrame) {
    if (!_pendingNavigationInfo) {
      _pendingNavigationInfo.reset(
          [[CRWWebControllerPendingNavigationInfo alloc] init]);
    }
    [_pendingNavigationInfo setMIMEType:response.response.MIMEType];
  }
}

- (void)commitPendingNavigationInfo {
  if ([_pendingNavigationInfo referrer]) {
    _currentReferrerString.reset([[_pendingNavigationInfo referrer] copy]);
  }
  if ([_pendingNavigationInfo MIMEType]) {
    self.webStateImpl->SetContentsMimeType(
        base::SysNSStringToUTF8([_pendingNavigationInfo MIMEType]));
  }
  [self updateCurrentBackForwardListItemHolder];

  _pendingNavigationInfo.reset();
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

- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder {
  web::NavigationItem* item = [self currentSessionEntry].navigationItemImpl;
  DCHECK(item);
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  DCHECK(holder);
  return holder;
}

- (void)updateCurrentBackForwardListItemHolder {
  // WebUI pages (which are loaded via loadHTMLString:baseURL:) have no entry
  // in the back/forward list, so the current item will still be the previous
  // page, and should not be associated.
  if (_webUIManager)
    return;

  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];

  WKNavigationType navigationType =
      _pendingNavigationInfo ? [_pendingNavigationInfo navigationType]
                             : WKNavigationTypeOther;
  holder->set_back_forward_list_item([_webView backForwardList].currentItem);
  holder->set_navigation_type(navigationType);
  holder->set_http_method([_pendingNavigationInfo HTTPMethod]);

  // Only update the MIME type in the holder if there was MIME type information
  // as part of this pending load. It will be nil when doing a fast
  // back/forward navigation, for instance, because the callback that would
  // populate it is not called in that flow.
  if ([_pendingNavigationInfo MIMEType])
    holder->set_mime_type([_pendingNavigationInfo MIMEType]);
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

  if (web::IsWKWebViewSSLCertError(error)) {
    // This could happen only if certificate is absent or could not be parsed.
    error = web::NetErrorFromError(error, net::ERR_SSL_SERVER_CERT_BAD_FORMAT);
#if defined(DEBUG)
    net::SSLInfo info;
    web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);
    CHECK(!error.cert);
#endif
  } else {
    error = web::NetErrorFromError(error);
  }

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

- (void)loadWithParams:(const NavigationManager::WebLoadParams&)originalParams {
  // Make a copy of |params|, as some of the delegate methods may modify it.
  NavigationManager::WebLoadParams params(originalParams);

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

  [self loadCurrentURL];
}

- (void)loadCurrentURL {
  // If the content view doesn't exist, the tab has either been evicted, or
  // never displayed. Bail, and let the URL be loaded when the tab is shown.
  if (!_containerView)
    return;

  // Reset current WebUI if one exists.
  [self clearWebUI];

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
    web::NavigationItem* item = [self currentNavItem];
    if (!(item->GetTransitionType() & ui::PAGE_TRANSITION_TYPED ||
          item->GetTransitionType() & ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
        self.sessionController.openedByDOM) {
      // WebUI URLs can not be opened by DOM to prevent cross-site scripting as
      // they have increased power. WebUI URLs may only be opened when the user
      // types in the URL or use bookmarks.
      [[self sessionController] discardNonCommittedEntries];
      return;
    } else {
      [self createWebUIForURL:currentURL];
    }
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

- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel);
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  return _documentURL;
}

- (BOOL)shouldLoadURLInNativeView:(const GURL&)url {
  // App-specific URLs that don't require WebUI are loaded in native views.
  return web::GetWebClient()->IsAppSpecificURL(url) &&
         !_webStateImpl->HasWebUI();
}

- (void)triggerPendingLoad {
  if (!_containerView) {
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
    _containerView.get().frame = containerViewFrame;
    DCHECK(!CGRectIsEmpty(_containerView.get().frame));

    [_containerView addGestureRecognizer:[self touchTrackingRecognizer]];
    [_containerView setAccessibilityIdentifier:web::kContainerViewID];
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
  } else if (_requireReloadOnDisplay && _webView) {
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
  base::RecordAction(UserMetricsAction("Reload"));
  if (_webView) {
    web::NavigationItem* transientItem =
        _webStateImpl->GetNavigationManagerImpl().GetTransientItem();
    if (transientItem) {
      // If there's a transient item, a reload is considered a new navigation to
      // the transient item's URL (as on other platforms).
      NavigationManager::WebLoadParams reloadParams(transientItem->GetURL());
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
  [self reloadInternal];
}

- (void)abortLoad {
  [_webView stopLoading];
  [_pendingNavigationInfo setCancelled:YES];
  _certVerificationErrors->Clear();
  [self loadCancelled];
}

- (void)loadCancelled {
  [_passKitDownloader cancelPendingDownload];

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
        _webStateImpl->SetIsLoading(false);
      }
      [_delegate webLoadCancelled:_URLOnStartLoading];
      break;
    case web::PAGE_LOADED:
      break;
  }
}

- (void)goToItemAtIndex:(int)index {
  CRWSessionController* sessionController = self.sessionController;
  NSArray* entries = sessionController.entries;
  DCHECK_LT(static_cast<NSUInteger>(index), entries.count);
  DCHECK_GE(index, 0);

  if (!_webStateImpl->IsShowingWebInterstitial())
    [self recordStateInHistory];
  CRWSessionEntry* fromEntry = self.sessionController.currentEntry;

  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  if ([userDefaults boolForKey:@"PendingIndexNavigationEnabled"]) {
    BOOL sameDocumentNavigation =
        [sessionController isSameDocumentNavigationBetweenEntry:fromEntry
                                                       andEntry:entries[index]];
    if (sameDocumentNavigation) {
      [self.sessionController goToEntryAtIndex:index];

      // Implementation of |webWillFinishHistoryNavigationFromEntry:| expects
      // that NavigationManager has either a pending item or already made the
      // navigation. Hence this delegate must be called after changing current
      // navigation item. TODO(crbug.com/670149): Remove this delegate method as
      // CRWWebController does not need to delegate setting Desktop User Agent.
      [_delegate webWillFinishHistoryNavigationFromEntry:fromEntry];
      [self updateHTML5HistoryState];
    } else {
      [sessionController discardNonCommittedEntries];
      [sessionController setPendingEntryIndex:index];

      // Implementation of |webWillFinishHistoryNavigationFromEntry:| expects
      // that NavigationManager has either a pending item or already made the
      // navigation. Hence this delegate must be called after changing pending
      // navigation index. TODO(crbug.com/670149): Remove this delegate method
      // as CRWWebController does not need to delegate setting Desktop User
      // Agent.
      [_delegate webWillFinishHistoryNavigationFromEntry:fromEntry];

      web::NavigationItemImpl* pendingItem =
          sessionController.pendingEntry.navigationItemImpl;
      pendingItem->SetTransitionType(ui::PageTransitionFromInt(
          pendingItem->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));

      [self loadCurrentURL];
    }
  } else {
    [self.sessionController goToEntryAtIndex:index];
    if (fromEntry)
      [self finishHistoryNavigationFromEntry:fromEntry];
  }
}

- (BOOL)isLoaded {
  return _loadPhase == web::PAGE_LOADED;
}

- (void)didFinishNavigation {
  // This can be called at multiple times after the document has loaded. Do
  // nothing if the document has already loaded.
  if (_loadPhase == web::PAGE_LOADED)
    return;
  [self loadCompleteWithSuccess:YES];
}

- (void)loadCompleteWithSuccess:(BOOL)loadSuccess {
  [self removePlaceholderOverlay];
  // The webView may have been torn down (or replaced by a native view). Be
  // safe and do nothing if that's happened.
  if (_loadPhase != web::PAGE_LOADING)
    return;

  DCHECK(_webView);

  const GURL currentURL([self currentURL]);

  _loadPhase = web::PAGE_LOADED;

  [self optOutScrollsToTopForSubviews];

  // Ensure the URL is as expected (and already reported to the delegate).
  // If |_lastRegisteredRequestURL| is invalid then |currentURL| will be
  // "about:blank".
  DCHECK((currentURL == _lastRegisteredRequestURL) ||
         (!_lastRegisteredRequestURL.is_valid() &&
          _documentURL.spec() == url::kAboutBlankURL))
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
  _webStateImpl->GetRequestTracker()->SetCacheModeFromUIThread(
      net::RequestTracker::CACHE_NORMAL);

  // Rather than creating a new WKBackForwardListItem when loading WebUI pages,
  // WKWebView will cache the WebUI HTML in the previous WKBackForwardListItem
  // since it's loaded via |-loadHTML:forURL:| instead of an NSURLRequest.  As a
  // result, the WebUI's HTML and URL will be loaded when navigating to that
  // WKBackForwardListItem, causing a mismatch between the visible content and
  // the visible URL (WebUI page will be visible, but URL will be the previous
  // page's URL).  To prevent this potential URL spoofing vulnerability, reset
  // the previous NavigationItem's WKBackForwardListItem to force loading via
  // NSURLRequest.
  if (_webUIManager) {
    web::NavigationItem* lastNavigationItem =
        self.sessionController.previousEntry.navigationItem;
    if (lastNavigationItem) {
      web::WKBackForwardListItemHolder* holder =
          web::WKBackForwardListItemHolder::FromNavigationItem(
              lastNavigationItem);
      DCHECK(holder);
      holder->set_back_forward_list_item(nil);
    }
  }

  [self restoreStateFromHistory];
  _webStateImpl->OnPageLoaded(currentURL, loadSuccess);
  _webStateImpl->SetIsLoading(false);
  // Inform the embedder the load completed.
  [_delegate webDidFinishWithURL:currentURL loadSuccess:loadSuccess];
}

- (void)goDelta:(int)delta {
  if (_isBeingDestroyed)
    return;

  if (delta == 0) {
    [self reload];
    return;
  }

  web::NavigationManagerImpl& navigationManager =
      _webStateImpl->GetNavigationManagerImpl();
  if (navigationManager.CanGoToOffset(delta)) {
    [self goToItemAtIndex:navigationManager.GetIndexForOffset(delta)];
  }
}

- (void)finishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry {
  [_delegate webWillFinishHistoryNavigationFromEntry:fromEntry];

  // Only load the new URL if it has a different document than |fromEntry| to
  // prevent extra page loads from NavigationItems created by hash changes or
  // calls to window.history.pushState().
  BOOL shouldLoadURL =
      ![_webStateImpl->GetNavigationManagerImpl().GetSessionController()
          isSameDocumentNavigationBetweenEntry:fromEntry
                                      andEntry:self.currentSessionEntry];
  web::NavigationItemImpl* currentItem =
      self.currentSessionEntry.navigationItemImpl;
  GURL endURL = [self URLForHistoryNavigationFromItem:fromEntry.navigationItem
                                               toItem:currentItem];
  if (shouldLoadURL) {
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_RELOAD | ui::PAGE_TRANSITION_FORWARD_BACK);

    NavigationManager::WebLoadParams params(endURL);
    if (currentItem) {
      params.referrer = currentItem->GetReferrer();
    }
    params.transition_type = transition;
    [self loadWithParams:params];
  }
  // If this is a same-document navigation, update the HTML5 history state
  // immediately.  For cross-document navigations, the history state will be
  // updated after the navigation is committed, as attempting to replace the URL
  // here will result in a JavaScript SecurityError due to the URLs having
  // different origins.
  if (!shouldLoadURL)
    [self updateHTML5HistoryState];
}

- (void)addGestureRecognizerToWebView:(UIGestureRecognizer*)recognizer {
  if ([_gestureRecognizers containsObject:recognizer])
    return;

  [_webView addGestureRecognizer:recognizer];
  [_gestureRecognizers addObject:recognizer];
}

- (void)removeGestureRecognizerFromWebView:(UIGestureRecognizer*)recognizer {
  if (![_gestureRecognizers containsObject:recognizer])
    return;

  [_webView removeGestureRecognizer:recognizer];
  [_gestureRecognizers removeObject:recognizer];
}

- (void)addToolbarViewToWebView:(UIView*)toolbarView {
  DCHECK(toolbarView);
  if ([_webViewToolbars containsObject:toolbarView])
    return;
  [_webViewToolbars addObject:toolbarView];
  if (_webView)
    [_containerView addToolbar:toolbarView];
}

- (void)removeToolbarViewFromWebView:(UIView*)toolbarView {
  if (![_webViewToolbars containsObject:toolbarView])
    return;
  [_webViewToolbars removeObject:toolbarView];
  if (_webView)
    [_containerView removeToolbar:toolbarView];
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

- (BOOL)useDesktopUserAgent {
  web::NavigationItem* item = [self currentNavItem];
  return item && item->IsOverridingUserAgent();
}

- (web::MojoFacade*)mojoFacade {
  if (!_mojoFacade) {
    service_manager::mojom::InterfaceProvider* interfaceProvider =
        _webStateImpl->GetMojoInterfaceRegistry();
    _mojoFacade.reset(new web::MojoFacade(interfaceProvider, self));
  }
  return _mojoFacade.get();
}

- (CRWPassKitDownloader*)passKitDownloader {
  if (_passKitDownloader) {
    return _passKitDownloader.get();
  }
  base::WeakNSObject<CRWWebController> weakSelf(self);
  web::PassKitCompletionHandler passKitCompletion = ^(NSData* data) {
    base::scoped_nsobject<CRWWebController> strongSelf([weakSelf retain]);
    if (!strongSelf) {
      return;
    }
    // Cancel load to update web state, since the PassKit download happens
    // through a separate flow. This follows the same flow as when PassKit is
    // downloaded through UIWebView.
    [strongSelf loadCancelled];
    SEL didLoadPassKitObject = @selector(webController:didLoadPassKitObject:);
    id<CRWWebDelegate> delegate = [strongSelf delegate];
    if ([delegate respondsToSelector:didLoadPassKitObject]) {
      [delegate webController:strongSelf didLoadPassKitObject:data];
    }
  };
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  _passKitDownloader.reset([[CRWPassKitDownloader alloc]
      initWithContextGetter:browserState->GetRequestContext()
          completionHandler:passKitCompletion]);
  return _passKitDownloader.get();
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

- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)completionHandler {
  NSString* safeScript = [self scriptByAddingWindowIDCheckForScript:script];
  web::ExecuteJavaScript(_webView, safeScript, completionHandler);
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_injectedScriptManagers containsObject:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  DCHECK(script.length);
  // Script execution is an asynchronous operation which may pass sensitive
  // data to the page. executeJavaScript:completionHandler makes sure that
  // receiver page did not change by checking its window id.
  // |[_webView executeJavaScript:completionHandler:]| is not used here because
  // it does not check that page is the same.
  [self executeJavaScript:script completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

#pragma mark -

- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(web::JavaScriptResultBlock)completion {
  [self setUserInteractionRegistered:YES];
  [self executeJavaScript:script completionHandler:completion];
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
    (*handlers)["geolocationDialog.suppressed"] =
        @selector(handleGeolocationDialogSuppressedMessage:context:);
    (*handlers)["document.favicons"] =
        @selector(handleDocumentFaviconsMessage:context:);
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

- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  // Broken out into separate method to catch errors.
  if (![self respondToWKScriptMessage:message]) {
    DLOG(WARNING) << "Message from JS not handled due to invalid format";
  }
}

- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script {
  NSString* kTemplate = @"if (__gCrWeb['windowId'] === '%@') { %@; }";
  return [NSString
      stringWithFormat:kTemplate, [_windowIDJSManager windowID], script];
}

- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage {
  if (!scriptMessage.frameInfo.mainFrame) {
    // Messages from iframes are not currently supported.
    return NO;
  }

  int errorCode = 0;
  std::string errorMessage;
  std::unique_ptr<base::Value> inputJSONData(
      base::JSONReader::ReadAndReturnError(
          base::SysNSStringToUTF8(scriptMessage.body), false, &errorCode,
          &errorMessage));
  if (errorCode) {
    DLOG(WARNING) << "JSON parse error: %s" << errorMessage.c_str();
    return NO;
  }
  base::DictionaryValue* message = nullptr;
  if (!inputJSONData->GetAsDictionary(&message)) {
    return NO;
  }
  std::string windowID;
  message->GetString("crwWindowId", &windowID);
  // Check for correct windowID
  if (base::SysNSStringToUTF8([_windowIDJSManager windowID]) != windowID) {
    DLOG(WARNING) << "Message from JS ignored due to non-matching windowID: " <<
        [_windowIDJSManager windowID]
                  << " != " << base::SysUTF8ToNSString(windowID);
    return NO;
  }
  base::DictionaryValue* command = nullptr;
  if (!message->GetDictionary("crwCommand", &command)) {
    return NO;
  }
  if ([scriptMessage.name isEqualToString:kScriptMessageName]) {
    return [self respondToMessage:command
                userIsInteracting:[self userIsInteracting]
                        originURL:net::GURLWithNSURL([_webView URL])];
  }

  NOTREACHED();
  return NO;
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

- (BOOL)handleGeolocationDialogSuppressedMessage:(base::DictionaryValue*)message
                                         context:(NSDictionary*)context {
  [self didSuppressDialog];
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
  bool unmediated = false;
  if (!message->GetBoolean("unmediated", &unmediated)) {
    DLOG(WARNING) << "JS message parameter not found: unmediated";
    return NO;
  }
  base::ListValue* federations_value = nullptr;
  if (!message->GetList("federations", &federations_value)) {
    DLOG(WARNING) << "JS message parameter not found: federations";
    return NO;
  }
  std::vector<std::string> federations;
  for (const auto& federation_value : *federations_value) {
    std::string federation;
    if (!federation_value->GetAsString(&federation)) {
      DLOG(WARNING) << "JS message parameter 'federations' contains wrong type";
      return NO;
    }
    federations.push_back(federation);
  }
  DCHECK(context[web::kUserIsInteractingKey]);
  _webStateImpl->OnCredentialsRequested(
      request_id, net::GURLWithNSURL(context[web::kOriginURLKey]), unmediated,
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
  // Because hash changes don't trigger |-didFinishNavigation|, fetch favicons
  // for the new page manually.
  [self executeJavaScript:@"__gCrWeb.sendFaviconsToHost();"
        completionHandler:nil];

  // Record that the current NavigationItem was created by a hash change, but
  // ignore hashchange events that are manually dispatched for same-document
  // navigations.
  if (_dispatchingSameDocumentHashChangeEvent) {
    _dispatchingSameDocumentHashChangeEvent = NO;
  } else {
    web::NavigationItemImpl* item =
        static_cast<web::NavigationItemImpl*>([self currentNavItem]);
    DCHECK(item);
    item->SetIsCreatedFromHashChange(true);
  }

  // Notify the observers.
  _webStateImpl->OnUrlHashChanged();
  return YES;
}

- (BOOL)handleWindowHistoryBackMessage:(base::DictionaryValue*)message
                               context:(NSDictionary*)context {
  [self goDelta:-1];
  return YES;
}

- (BOOL)handleWindowHistoryForwardMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context {
  [self goDelta:1];
  return YES;
}

- (BOOL)handleWindowHistoryGoMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  int delta = 0;
  if (message->GetInteger("value", &delta)) {
    [self goDelta:delta];
    return YES;
  }
  return NO;
}

- (BOOL)handleWindowHistoryWillChangeStateMessage:(base::DictionaryValue*)unused
                                          context:(NSDictionary*)unusedContext {
  _changingHistoryState = YES;
  return YES;
}

- (BOOL)handleWindowHistoryDidPushStateMessage:(base::DictionaryValue*)message
                                       context:(NSDictionary*)context {
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;

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
      [self javaScriptToReplaceWebViewURL:pushURL stateObjectJSON:stateObject];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self executeJavaScript:replaceWebViewJS completionHandler:^(id, NSError*) {
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
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;

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
  NSString* replaceStateJS = [self javaScriptToReplaceWebViewURL:replaceURL
                                                 stateObjectJSON:stateObject];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self executeJavaScript:replaceStateJS completionHandler:^(id, NSError*) {
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
  if (flag)
    _interactionRegisteredSinceLastURLChange = YES;
}

- (BOOL)userInteractionRegistered {
  return _userInteractionRegistered;
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
- (BOOL)shouldAllowLoadWithNavigationAction:(WKNavigationAction*)action {
  NSURLRequest* request = action.request;
  GURL requestURL = net::GURLWithNSURL(request.URL);

  // External application launcher needs |isNavigationTypeLinkActivated| to
  // decide if the user intended to open the application by clicking on a link.
  BOOL isNavigationTypeLinkActivated =
      action.navigationType == WKNavigationTypeLinkActivated;

  // Check if the request should be delayed.
  if (_externalRequest && _externalRequest->url == requestURL) {
    // Links that can't be shown in a tab by Chrome but can be handled by
    // external apps (e.g. tel:, mailto:) are opened directly despite the target
    // attribute on the link. We don't open a new tab for them because Mobile
    // Safari doesn't do that (and sites are expecting us to do the same) and
    // also because there would be nothing shown in that new tab; it would
    // remain on about:blank (see crbug.com/240178)
    if ([CRWWebController webControllerCanShow:requestURL] ||
        ![_delegate openExternalURL:requestURL
                        linkClicked:isNavigationTypeLinkActivated]) {
      web::NewWindowInfo windowInfo = *_externalRequest;
      dispatch_async(dispatch_get_main_queue(), ^{
        [self openPopupWithInfo:windowInfo];
      });
    }
    _externalRequest.reset();
    return NO;
  }

  // Check if the link navigation leads to a launch of an external app.
  // TODO(crbug.com/607780): Revise the logic of allowing external app launch
  // and move it to externalAppLauncher.
  BOOL isOpenInNewTabNavigation =
      !_webStateImpl->GetNavigationManager()->GetItemCount();
  BOOL isPossibleLinkClick = [self isLinkNavigation:action.navigationType];
  if (isPossibleLinkClick || isOpenInNewTabNavigation ||
      [self currentTransition] == ui::PAGE_TRANSITION_AUTO_BOOKMARK) {
    // Check If the URL is handled by a native app.
    if ([self urlTriggersNativeAppLaunch:requestURL
                               sourceURL:[self currentNavigationURL]
                 linkActivatedNavigation:isNavigationTypeLinkActivated]) {
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
  DCHECK(_webView);
  if (![self shouldOpenURL:requestURL
           mainDocumentURL:mainDocumentURL
               linkClicked:isPossibleLinkClick]) {
    return NO;
  }

  // If the URL doesn't look like one we can show, try to open the link with an
  // external application.
  // TODO(droger):  Check transition type before opening an external
  // application? For example, only allow it for TYPED and LINK transitions.
  if (![CRWWebController webControllerCanShow:requestURL]) {
    if (![self shouldOpenExternalURLForNavigationAction:action]) {
      return NO;
    }

    // Stop load if navigation is believed to be happening on the main frame.
    if ([self isMainFrameNavigationAction:action])
      [self stopLoading];

    // Purge web view if last committed URL is different from the document URL.
    // This can happen if external URL was added to the navigation stack and was
    // loaded using Go Back or Go Forward navigation (in which case document URL
    // will point to the previous page).  If this is the first load for a
    // NavigationManager, there will be no last committed item, so check here.
    web::NavigationItem* lastCommittedItem =
        self.webState->GetNavigationManager()->GetLastCommittedItem();
    if (lastCommittedItem) {
      GURL lastCommittedURL = lastCommittedItem->GetURL();
      if (lastCommittedURL != _documentURL) {
        [self requirePageReconstruction];
        [self setDocumentURL:lastCommittedURL];
      }
    }

    if ([_delegate openExternalURL:requestURL
                       linkClicked:isNavigationTypeLinkActivated]) {
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

- (void)handleLoadError:(NSError*)error inMainFrame:(BOOL)inMainFrame {
  NSString* MIMEType = [_pendingNavigationInfo MIMEType];
  if ([_passKitDownloader isMIMETypePassKitType:MIMEType])
    return;
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

  // Reset SSL status to default, unless the load was cancelled (manually or by
  // back-forward navigation).
  web::NavigationManager* navManager = self.webState->GetNavigationManager();
  if (navManager->GetLastCommittedItem() && [error code] != NSURLErrorCancelled)
    navManager->GetLastCommittedItem()->GetSSL() = web::SSLStatus();

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

- (void)handleCancelledError:(NSError*)error {
  if ([self shouldCancelLoadForCancelledError:error]) {
    [self loadCancelled];
    [[self sessionController] discardNonCommittedEntries];
  }
}

- (BOOL)shouldCancelLoadForCancelledError:(NSError*)error {
  DCHECK_EQ(error.code, NSURLErrorCancelled);
  // Do not cancel the load if it is for an app specific URL, as such errors
  // are produced during the app specific URL load process.
  const GURL errorURL =
      net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
  if (web::GetWebClient()->IsAppSpecificURL(errorURL))
    return NO;

  if (self.sessionController.pendingEntryIndex != -1 &&
      ![self isLoadRequestPendingForURL:errorURL]) {
    // Do not cancel the load if there is a pending back forward navigation for
    // another URL (Back forward happened in the middle of WKWebView
    // navigation).
    return NO;
  }

  // Don't cancel NSURLErrorCancelled errors originating from navigation
  // as the WKWebView will automatically retry these loads.
  WKWebViewErrorSource source = WKWebViewErrorSourceFromError(error);
  return source != NAVIGATION;
}

#pragma mark -
#pragma mark WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  _webStateImpl->CreateWebUI(URL);
  _webUIManager.reset(
      [[CRWWebUIManager alloc] initWithWebState:self.webStateImpl]);
}

- (void)clearWebUI {
  _webStateImpl->ClearWebUI();
  _webUIManager.reset();
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

  // This is custom long press gesture recognizer. By the time the gesture is
  // recognized the web controller needs to know if there is a link under the
  // touch. If there a link, the web controller will reject system's context
  // menu and show another one. If for some reason context menu info is not
  // fetched - system context menu will be shown.
  [self setDOMElementForLastTouch:nil];
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [self fetchDOMElementAtPoint:[touch locationInView:_webView]
             completionHandler:^(NSDictionary* element) {
               [weakSelf setDOMElementForLastTouch:element];
             }];
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);
  if (!_webView) {
    // Show the context menu iff currently displaying a web view.
    // Do nothing for native views.
    return NO;
  }

  // Fetching is considered as successful even if |_DOMElementForLastTouch| is
  // empty. However if |_DOMElementForLastTouch| is empty then custom context
  // menu should not be shown.
  UMA_HISTOGRAM_BOOLEAN("WebController.FetchContextMenuInfoAsyncSucceeded",
                        !!_DOMElementForLastTouch);
  return [_DOMElementForLastTouch count];
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
  // This is a UIWebView callback, which is no longer called.
  NOTREACHED();
}

- (void)updatedProgress:(float)progress {
  // This is a UIWebView callback, which is no longer called.
  NOTREACHED();
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
#pragma mark Auth Challenge

- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler {
  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  if (policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER) {
    // Cert is invalid, but user agreed to proceed, override default behavior.
    completionHandler(NSURLSessionAuthChallengeUseCredential,
                      [NSURLCredential credentialForTrust:trust]);
    return;
  }

  if (policy != web::CERT_ACCEPT_POLICY_ALLOW &&
      SecTrustGetCertificateCount(trust)) {
    // The cert is invalid and the user has not agreed to proceed. Cache the
    // cert verification result in |_certVerificationErrors|, so that it can
    // later be reused inside |didFailProvisionalNavigation:|.
    // The leaf cert is used as the key, because the chain provided by
    // |didFailProvisionalNavigation:| will differ (it is the server-supplied
    // chain), thus if intermediates were considered, the keys would mismatch.
    scoped_refptr<net::X509Certificate> leafCert =
        net::X509Certificate::CreateFromHandle(
            SecTrustGetCertificateAtIndex(trust, 0),
            net::X509Certificate::OSCertHandles());
    if (leafCert) {
      BOOL is_recoverable =
          policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
      std::string host =
          base::SysNSStringToUTF8(challenge.protectionSpace.host);
      _certVerificationErrors->Put(
          web::CertHostPair(leafCert, host),
          CertVerificationError(is_recoverable, certStatus));
    }
  }
  completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler {
  NSURLProtectionSpace* space = challenge.protectionSpace;
  DCHECK(
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPDigest]);

  if (self.shouldSuppressDialogs) {
    [self didSuppressDialog];
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  SEL selector = @selector(webController:
         runAuthDialogForProtectionSpace:
                      proposedCredential:
                       completionHandler:);
  if (![self.UIDelegate respondsToSelector:selector]) {
    // Embedder does not support HTTP Authentication.
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  [self.UIDelegate webController:self
      runAuthDialogForProtectionSpace:space
                   proposedCredential:challenge.proposedCredential
                    completionHandler:^(NSString* user, NSString* password) {
                      [CRWWebController
                          processHTTPAuthForUser:user
                                        password:password
                               completionHandler:completionHandler];
                    }];
}

+ (void)processHTTPAuthForUser:(NSString*)user
                      password:(NSString*)password
             completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                         NSURLCredential*))completionHandler {
  DCHECK_EQ(user == nil, password == nil);
  if (!user || !password) {
    // Embedder cancelled authentication.
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }
  completionHandler(
      NSURLSessionAuthChallengeUseCredential,
      [NSURLCredential
          credentialWithUser:user
                    password:password
                 persistence:NSURLCredentialPersistenceForSession]);
}

#pragma mark -
#pragma mark JavaScript Dialog

- (void)runJavaScriptDialogOfType:(web::JavaScriptDialogType)type
                 initiatedByFrame:(WKFrameInfo*)frame
                          message:(NSString*)message
                      defaultText:(NSString*)defaultText
                       completion:(void (^)(BOOL, NSString*))completionHandler {
  self.webStateImpl->RunJavaScriptDialog(
      net::GURLWithNSURL(frame.request.URL), type, message, defaultText,
      base::BindBlock(^(bool success, NSString* input) {
        completionHandler(success, input);
      }));
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

- (void)addActivityIndicatorTask {
  [[CRWNetworkActivityIndicatorManager sharedInstance]
      startNetworkTaskForGroup:[self activityIndicatorGroupID]];
}

- (void)clearActivityIndicatorTasks {
  [[CRWNetworkActivityIndicatorManager sharedInstance]
      clearNetworkTasksForGroup:[self activityIndicatorGroupID]];
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
  [_containerView addSubview:_placeholderOverlayView];

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
    if (_webView) {
      [[self view] addSubview:_webView];
    }
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

  base::WeakNSObject<UIScrollView> weakScrollView(self.webScrollView);
  [self extractViewportTagWithCompletion:^(
            const web::PageViewportState* viewportState) {
    if (!weakScrollView)
      return;
    base::scoped_nsobject<UIScrollView> scrollView([weakScrollView retain]);
    if (viewportState && !viewportState->viewport_tag_present() &&
        [scrollView minimumZoomScale] == [scrollView maximumZoomScale] &&
        [scrollView zoomScale] > 1.0) {
      UMA_HISTOGRAM_BOOLEAN(kUMAViewportZoomBugCount, true);
    }
  }];
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
    if (!zoomState.IsValid())
      zoomState = web::PageZoomState(1.0, 1.0, 1.0);
    [self applyWebViewScrollZoomScaleFromZoomState:zoomState];
    _applyingPageState = NO;
  }
}

#pragma mark -
#pragma mark CRWWebViewScrollViewProxyObserver

// Under WKWebView, JavaScript can execute asynchronously. User can start
// scrolling and calls to window.scrollTo executed during scrolling will be
// treated as "during user interaction" and can cause app to go fullscreen.
// This is a workaround to use this webViewScrollViewIsDragging flag to ignore
// window.scrollTo while user is scrolling. See crbug.com/554257
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(true)"
        completionHandler:nil];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  [self executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(false)"
        completionHandler:nil];
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
  if (_webView) {
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
  if (_webView) {
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

- (void)extractViewportTagWithCompletion:(ViewportStateCompletion)completion {
  DCHECK(completion);
  web::NavigationItem* currentItem = [self currentNavItem];
  if (!currentItem) {
    completion(nullptr);
    return;
  }
  NSString* const kViewportContentQuery =
      @"var viewport = document.querySelector('meta[name=\"viewport\"]');"
       "viewport ? viewport.content : '';";
  base::WeakNSObject<CRWWebController> weakSelf(self);
  int itemID = currentItem->GetUniqueID();
  [self executeJavaScript:kViewportContentQuery
        completionHandler:^(id viewportContent, NSError*) {
          web::NavigationItem* item = [weakSelf currentNavItem];
          if (item && item->GetUniqueID() == itemID) {
            web::PageViewportState viewportState(
                base::mac::ObjCCast<NSString>(viewportContent));
            completion(&viewportState);
          } else {
            completion(nullptr);
          }
        }];
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
  if (!_webView)
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
  [self extractViewportTagWithCompletion:^(
            const web::PageViewportState* viewportState) {
    if (viewportState) {
      [weakSelf applyPageDisplayState:displayStateCopy
                         userScalable:viewportState->user_scalable()];
    }
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
  id webView = _webView;
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
  id webView = _webView;
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
  // After rendering a web page, WKWebView keeps the |minimumZoomScale| and
  // |maximumZoomScale| properties of its scroll view constant while adjusting
  // the |zoomScale| property accordingly.  The maximum-scale or minimum-scale
  // meta tags of a page may have changed since the state was recorded, so clamp
  // the zoom scale to the current range if necessary.
  DCHECK(zoomState.IsValid());
  CGFloat zoomScale = zoomState.zoom_scale();
  if (zoomScale < self.webScrollView.minimumZoomScale)
    zoomScale = self.webScrollView.minimumZoomScale;
  if (zoomScale > self.webScrollView.maximumZoomScale)
    zoomScale = self.webScrollView.maximumZoomScale;
  self.webScrollView.zoomScale = zoomScale;
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

- (void)fetchWebPageWidthWithCompletionHandler:(void (^)(CGFloat))handler {
  if (!_webView) {
    handler(0);
    return;
  }

  [self executeJavaScript:@"__gCrWeb.getPageWidth();"
        completionHandler:^(id pageWidth, NSError*) {
          handler([base::mac::ObjCCastStrict<NSNumber>(pageWidth) floatValue]);
        }];
}

- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler {
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
    [weakSelf executeJavaScript:kGetElementScript
              completionHandler:^(id element, NSError*) {
                handler(base::mac::ObjCCastStrict<NSDictionary>(element));
              }];
  }];
}

- (web::ContextMenuParams)contextMenuParamsForElement:(NSDictionary*)element {
  web::ContextMenuParams params;
  NSString* title = nil;
  NSString* href = element[@"href"];
  if (href) {
    params.link_url = GURL(base::SysNSStringToUTF8(href));
    if (params.link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      base::string16 URLText = url_formatter::FormatUrl(params.link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }
  NSString* src = element[@"src"];
  if (src) {
    params.src_url = GURL(base::SysNSStringToUTF8(src));
    if (!title)
      title = [[src copy] autorelease];
    if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)])
      title = nil;
  }
  NSString* titleAttribute = element[@"title"];
  if (titleAttribute)
    title = titleAttribute;
  if (title) {
    params.menu_title.reset([title copy]);
  }
  NSString* referrerPolicy = element[@"referrerPolicy"];
  if (referrerPolicy) {
    params.referrer_policy =
        web::ReferrerPolicyFromString(base::SysNSStringToUTF8(referrerPolicy));
  }
  NSString* innerText = element[@"innerText"];
  if ([innerText length] > 0) {
    params.link_text.reset([innerText copy]);
  }
  return params;
}

#pragma mark -
#pragma mark Fullscreen

- (CGRect)visibleFrame {
  CGRect frame = [_containerView bounds];
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

- (void)didSuppressDialog {
  if ([_delegate respondsToSelector:@selector(webControllerDidSuppressDialog:)])
    [_delegate webControllerDidSuppressDialog:self];
}

- (void)didBlockPopupWithURL:(GURL)popupURL
                   sourceURL:(GURL)sourceURL
              referrerPolicy:(const std::string&)referrerPolicyString {
  web::ReferrerPolicy referrerPolicy =
      web::ReferrerPolicyFromString(referrerPolicyString);
  web::Referrer referrer(sourceURL, referrerPolicy);
  NSString* const kWindowName = @"";  // obsoleted
  base::WeakNSObject<CRWWebController> weakSelf(self);
  void (^showPopupHandler)() = ^{
    // On Desktop cross-window comunication is not supported for unblocked
    // popups; so it's ok to create a new independent page.
    CRWWebController* child =
        [[weakSelf delegate] webPageOrderedOpen:popupURL
                                       referrer:referrer
                                     windowName:kWindowName
                                   inBackground:NO];
    DCHECK(!child || child.sessionController.openedByDOM);
  };

  web::BlockedPopupInfo info(popupURL, referrer, kWindowName, showPopupHandler);
  [self.delegate webController:self didBlockPopup:info];
}

- (void)didBlockPopupWithURL:(GURL)popupURL sourceURL:(GURL)sourceURL {
  if ([_delegate respondsToSelector:@selector(webController:didBlockPopup:)]) {
    base::WeakNSObject<CRWWebController> weakSelf(self);
    dispatch_async(dispatch_get_main_queue(), ^{
      [self queryPageReferrerPolicy:^(NSString* policy) {
        [weakSelf didBlockPopupWithURL:popupURL
                             sourceURL:sourceURL
                        referrerPolicy:base::SysNSStringToUTF8(policy)];
      }];
    });
  }
}

- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action {
  if (action.targetFrame) {
    return action.targetFrame.mainFrame;
  }
  // According to WKNavigationAction documentation, in the case of a new window
  // navigation, target frame will be nil. In this case check if the
  // |sourceFrame| is the mainFrame.
  return action.sourceFrame.mainFrame;
}

- (BOOL)shouldOpenExternalURLForNavigationAction:(WKNavigationAction*)action {
  ExternalURLRequestStatus requestStatus = NUM_EXTERNAL_URL_REQUEST_STATUS;
  if ([self isMainFrameNavigationAction:action]) {
    requestStatus = MAIN_FRAME_ALLOWED;
  } else {
    // If the request's main document URL differs from that at the time of the
    // last user interaction, then the page has changed since the user last
    // interacted.
    BOOL userInteractedWithRequestMainFrame =
        [self userClickedRecently] &&
        net::GURLWithNSURL(action.request.mainDocumentURL) ==
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

  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  return [_delegate respondsToSelector:@selector(webController:
                                           shouldOpenExternalURL:)] &&
         [_delegate webController:self shouldOpenExternalURL:requestURL];
}

- (BOOL)urlTriggersNativeAppLaunch:(const GURL&)URL
                         sourceURL:(const GURL&)sourceURL
           linkActivatedNavigation:(BOOL)linkActivatedNavigation {
  if (![_delegate respondsToSelector:@selector(urlTriggersNativeAppLaunch:
                                                                sourceURL:
                                                              linkClicked:)]) {
    return NO;
  }
  return [_delegate urlTriggersNativeAppLaunch:URL
                                     sourceURL:sourceURL
                                   linkClicked:linkActivatedNavigation];
}

- (CGFloat)headerHeight {
  if (![_delegate respondsToSelector:@selector(headerHeightForWebController:)])
    return 0.0f;
  return [_delegate headerHeightForWebController:self];
}

- (void)didUpdateHistoryStateWithPageURL:(const GURL&)url {
  _webStateImpl->GetRequestTracker()->HistoryStateChange(url);
  [_delegate webDidUpdateHistoryStateWithPageURL:url];
}

- (void)updateSSLStatusForCurrentNavigationItem {
  if (_isBeingDestroyed) {
    return;
  }

  web::NavigationManager* navManager = self.webState->GetNavigationManager();
  web::NavigationItem* currentNavItem = navManager->GetLastCommittedItem();
  if (!currentNavItem) {
    return;
  }

  if (!_SSLStatusUpdater) {
    _SSLStatusUpdater.reset([[CRWSSLStatusUpdater alloc]
        initWithDataSource:self
         navigationManager:navManager]);
    [_SSLStatusUpdater setDelegate:self];
  }
  NSString* host = base::SysUTF8ToNSString(_documentURL.host());
  BOOL hasOnlySecureContent = [_webView hasOnlySecureContent];
  base::ScopedCFTypeRef<SecTrustRef> trust;
  if (base::ios::IsRunningOnIOS10OrLater()) {
    trust.reset([_webView serverTrust], base::scoped_policy::RETAIN);
  } else {
    trust = web::CreateServerTrustFromChain([_webView certificateChain], host);
  }

  [_SSLStatusUpdater updateSSLStatusForNavigationItem:currentNavItem
                                         withCertHost:host
                                                trust:std::move(trust)
                                 hasOnlySecureContent:hasOnlySecureContent];
}

- (void)didUpdateSSLStatusForCurrentNavigationItem {
  if ([_delegate respondsToSelector:
          @selector(
              webControllerDidUpdateSSLStatusForCurrentNavigationItem:)]) {
    [_delegate webControllerDidUpdateSSLStatusForCurrentNavigationItem:self];
  }
}

- (void)handleSSLCertError:(NSError*)error {
  CHECK(web::IsWKWebViewSSLCertError(error));

  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);

  if (!info.cert) {
    // |info.cert| can be null if certChain in NSError is empty or can not be
    // parsed, in this case do not ask delegate if error should be allowed, it
    // should not be.
    [self handleLoadError:error inMainFrame:YES];
    return;
  }

  // Retrieve verification results from _certVerificationErrors cache to avoid
  // unnecessary recalculations. Verification results are cached for the leaf
  // cert, because the cert chain in |didReceiveAuthenticationChallenge:| is
  // the OS constructed chain, while |chain| is the chain from the server.
  NSArray* chain = error.userInfo[web::kNSErrorPeerCertificateChainKey];
  NSURL* requestURL = error.userInfo[web::kNSErrorFailingURLKey];
  NSString* host = [requestURL host];
  scoped_refptr<net::X509Certificate> leafCert;
  BOOL recoverable = NO;
  if (chain.count && host.length) {
    // The complete cert chain may not be available, so the leaf cert is used
    // as a key to retrieve _certVerificationErrors, as well as for storing the
    // cert decision.
    leafCert = web::CreateCertFromChain(@[ chain.firstObject ]);
    if (leafCert) {
      auto error = _certVerificationErrors->Get(
          {leafCert, base::SysNSStringToUTF8(host)});
      bool cacheHit = error != _certVerificationErrors->end();
      if (cacheHit) {
        recoverable = error->second.is_recoverable;
        info.cert_status = error->second.status;
      }
      UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                            cacheHit);
    }
  }

  // Ask web client if this cert error should be allowed.
  web::GetWebClient()->AllowCertificateError(
      self.webState, net::MapCertStatusToNetError(info.cert_status), info,
      net::GURLWithNSURL(requestURL), recoverable,
      base::BindBlock(^(bool proceed) {
        if (proceed) {
          DCHECK(recoverable);
          [_certVerificationController allowCert:leafCert
                                         forHost:host
                                          status:info.cert_status];
          [self loadCurrentURL];
        }
      }));

  [self didUpdateSSLStatusForCurrentNavigationItem];
  [self loadCancelled];
}

- (void)ensureWebViewCreated {
  WKWebViewConfiguration* config =
      [self webViewConfigurationProvider].GetWebViewConfiguration();
  [self ensureWebViewCreatedWithConfiguration:config];
}

- (void)ensureWebViewCreatedWithConfiguration:(WKWebViewConfiguration*)config {
  if (!_webView) {
    [self setWebView:[self createWebViewWithConfiguration:config]];
    // The following is not called in -setWebView: as the latter used in unit
    // tests with fake web view, which cannot be added to view hierarchy.
    CHECK(_webUsageEnabled) << "Tried to create a web view while suspended!";

    DCHECK(_webView);

    [_webView setTag:kWebViewTag];
    [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight];
    [_webView setBackgroundColor:[UIColor colorWithWhite:0.2 alpha:1.0]];

    // Create a dependency between the |webView| pan gesture and BVC side swipe
    // gestures. Note: This needs to be added before the longPress recognizers
    // below, or the longPress appears to deadlock the remaining recognizers,
    // thereby breaking scroll.
    NSSet* recognizers = [_swipeRecognizerProvider swipeRecognizers];
    for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
      [self.webScrollView.panGestureRecognizer
          requireGestureRecognizerToFail:swipeRecognizer];
    }

    // On iOS 4.x, there are two gesture recognizers on the UIWebView
    // subclasses,
    // that have a minimum tap threshold of 0.12s and 0.75s.
    //
    // My theory is that the shorter threshold recognizer performs the link
    // highlight (grey highlight around links when it is tapped and held) while
    // the longer threshold one pops up the context menu.
    //
    // To override the context menu, this recognizer needs to react faster than
    // the 0.75s one. The below gesture recognizer is initialized with a
    // detection duration a little lower than that (see
    // kLongPressDurationSeconds). It also points the delegate to this class
    // that
    // allows simultaneously operate along with the other recognizers.
    _contextMenuRecognizer.reset([[UILongPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(showContextMenu:)]);
    [_contextMenuRecognizer setMinimumPressDuration:kLongPressDurationSeconds];
    [_contextMenuRecognizer setAllowableMovement:kLongPressMoveDeltaPixels];
    [_contextMenuRecognizer setDelegate:self];
    [_webView addGestureRecognizer:_contextMenuRecognizer];
    // Certain system gesture handlers are known to conflict with our context
    // menu handler, causing extra events to fire when the context menu is
    // active.

    // A number of solutions have been investigated. The lowest-risk solution
    // appears to be to recurse through the web controller's recognizers,
    // looking
    // for fingerprints of the recognizers known to cause problems, which are
    // then
    // de-prioritized (below our own long click handler).
    // Hunting for description fragments of system recognizers is undeniably
    // brittle for future versions of iOS. If it does break the context menu
    // events may leak (regressing b/5310177), but the app will otherwise work.
    [CRWWebController requireGestureRecognizerToFail:_contextMenuRecognizer
                                              inView:_webView
                               containingDescription:
                                   @"action=_highlightLongPressRecognized:"];

    // Add all additional gesture recognizers to the web view.
    for (UIGestureRecognizer* recognizer in _gestureRecognizers.get()) {
      [_webView addGestureRecognizer:recognizer];
    }

    _URLOnStartLoading = _defaultURL;

    // Add the web toolbars.
    [_containerView addToolbars:_webViewToolbars];

    base::scoped_nsobject<CRWWebViewContentView> webViewContentView(
        [[CRWWebViewContentView alloc] initWithWebView:_webView
                                            scrollView:self.webScrollView]);
    [_containerView displayWebViewContentView:webViewContentView];
  }
}

- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config {
  return [web::CreateWKWebView(CGRectZero, config,
                               self.webStateImpl->GetBrowserState(),
                               [self useDesktopUserAgent]) autorelease];
}

- (void)setWebView:(WKWebView*)webView {
  DCHECK_NE(_webView.get(), webView);

  // Unwind the old web view.
  // TODO(eugenebut): Remove CRWWKScriptMessageRouter once crbug.com/543374 is
  // fixed.
  CRWWKScriptMessageRouter* messageRouter =
      [self webViewConfigurationProvider].GetScriptMessageRouter();
  if (_webView) {
    [messageRouter removeAllScriptMessageHandlersForWebView:_webView];
  }
  [_webView setNavigationDelegate:nil];
  [_webView setUIDelegate:nil];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView removeObserver:self forKeyPath:keyPath];
  }
  [self clearActivityIndicatorTasks];

  _webView.reset([webView retain]);

  // Set up the new web view.
  if (webView) {
    base::WeakNSObject<CRWWebController> weakSelf(self);
    [messageRouter setScriptMessageHandler:^(WKScriptMessage* message) {
      [weakSelf didReceiveScriptMessage:message];
    }
                                      name:kScriptMessageName
                                   webView:webView];
    _windowIDJSManager.reset(
        [[CRWJSWindowIDManager alloc] initWithWebView:webView]);
  } else {
    _windowIDJSManager.reset();
  }
  [_webView setNavigationDelegate:self];
  [_webView setUIDelegate:self];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
  _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  [self setDocumentURL:_defaultURL];
}

- (void)removeWebViewAllowingCachedReconstruction:(BOOL)allowCache {
  if (!_webView)
    return;

  SEL cancelDialogsSelector = @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];
  _webStateImpl->CancelDialogs();

  if (allowCache)
    _expectedReconstructionURL = [self currentNavigationURL];
  else
    _expectedReconstructionURL = GURL();

  [self abortLoad];
  [_webView removeFromSuperview];
  [_containerView resetContent];
  [self setWebView:nil];
}

- (void)webViewWebProcessDidCrash {
  _webProcessIsDead = YES;

  SEL cancelDialogsSelector = @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];
  _webStateImpl->CancelDialogs();

  SEL rendererCrashSelector = @selector(webControllerWebProcessDidCrash:);
  if ([self.delegate respondsToSelector:rendererCrashSelector])
    [self.delegate webControllerWebProcessDidCrash:self];
}

- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider {
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  return web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
}

- (web::WebViewDocumentType)webViewDocumentType {
  // This happens during tests.
  if (!_webView) {
    return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
  }

  std::string MIMEType = self.webState->GetContentsMimeType();
  return [self documentTypeFromMIMEType:base::SysUTF8ToNSString(MIMEType)];
}

- (void)loadRequest:(NSMutableURLRequest*)request {
  _latestWKNavigation.reset([[_webView loadRequest:request] retain]);
}

- (void)loadPOSTRequest:(NSMutableURLRequest*)request {
  if (!_POSTRequestLoader) {
    _POSTRequestLoader.reset([[CRWJSPOSTRequestLoader alloc] init]);
  }

  CRWWKScriptMessageRouter* messageRouter =
      [self webViewConfigurationProvider].GetScriptMessageRouter();

  [_POSTRequestLoader loadPOSTRequest:request
                            inWebView:_webView
                        messageRouter:messageRouter
                    completionHandler:^(NSError* loadError) {
                      if (loadError)
                        [self handleLoadError:loadError inMainFrame:YES];
                      else
                        self.webStateImpl->SetContentsMimeType("text/html");
                    }];
}

- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL {
  // Remove the transient content view.
  [self clearTransientContentView];

  _loadPhase = web::LOAD_REQUESTED;

  // Web View should not be created for App Specific URLs.
  if (!web::GetWebClient()->IsAppSpecificURL(URL)) {
    [self ensureWebViewCreated];
    DCHECK(_webView) << "_webView null while trying to load HTML";
  }
  [_webView loadHTMLString:HTML baseURL:net::NSURLWithGURL(URL)];
}

- (void)loadHTML:(NSString*)HTML forAppSpecificURL:(const GURL&)URL {
  CHECK(web::GetWebClient()->IsAppSpecificURL(URL));
  [self loadHTML:HTML forURL:URL];
}

- (void)loadHTMLForCurrentURL:(NSString*)HTML {
  [self loadHTML:HTML forURL:self.currentURL];
}

- (void)stopLoading {
  _stoppedWKNavigation.reset(_latestWKNavigation);

  base::RecordAction(UserMetricsAction("Stop"));
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

#pragma mark -
#pragma mark WKUIDelegate Methods

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)action
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  if (self.shouldSuppressDialogs) {
    [self didSuppressDialog];
    return nil;
  }

  GURL requestURL = net::GURLWithNSURL(action.request.URL);

  // Don't create windows for non-empty invalid URLs.
  if (!requestURL.is_empty() && !requestURL.is_valid()) {
    DLOG(WARNING) << "Unable to open a window with invalid URL: "
                  << requestURL.spec();
    return nil;
  }

  if (![self userIsInteracting]) {
    NSString* referer = [self refererFromNavigationAction:action];
    GURL referrerURL =
        referer ? GURL(base::SysNSStringToUTF8(referer)) : [self currentURL];
    if ([self shouldBlockPopupWithURL:requestURL sourceURL:referrerURL]) {
      [self didBlockPopupWithURL:requestURL sourceURL:referrerURL];
      // Desktop Chrome does not return a window for the blocked popups;
      // follow the same approach by returning nil;
      return nil;
    }
  }

  CRWWebController* child = [self createChildWebController];
  // WKWebView requires WKUIDelegate to return a child view created with
  // exactly the same |configuration| object (exception is raised if config is
  // different). |configuration| param and config returned by
  // WKWebViewConfigurationProvider are different objects because WKWebView
  // makes a shallow copy of the config inside init, so every WKWebView
  // owns a separate shallow copy of WKWebViewConfiguration.
  [child ensureWebViewCreatedWithConfiguration:configuration];
  return child.webView;
}

- (void)webViewDidClose:(WKWebView*)webView {
  if (self.sessionController.openedByDOM) {
    [self.delegate webPageOrderedClose];
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                      initiatedByFrame:(WKFrameInfo*)frame
                     completionHandler:(void (^)())completionHandler {
  if (self.shouldSuppressDialogs) {
    [self didSuppressDialog];
    completionHandler();
    return;
  }

  SEL alertSelector = @selector(webController:
           runJavaScriptAlertPanelWithMessage:
                                   requestURL:
                            completionHandler:);
  if ([self.UIDelegate respondsToSelector:alertSelector]) {
    [self.UIDelegate webController:self
        runJavaScriptAlertPanelWithMessage:message
                                requestURL:net::GURLWithNSURL(frame.request.URL)
                         completionHandler:completionHandler];
  } else {
    [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_ALERT
                   initiatedByFrame:frame
                            message:message
                        defaultText:nil
                         completion:^(BOOL, NSString*) {
                           completionHandler();
                         }];
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                        initiatedByFrame:(WKFrameInfo*)frame
                       completionHandler:
                           (void (^)(BOOL result))completionHandler {
  if (self.shouldSuppressDialogs) {
    [self didSuppressDialog];
    completionHandler(NO);
    return;
  }

  SEL confirmationSelector = @selector(webController:
                runJavaScriptConfirmPanelWithMessage:
                                          requestURL:
                                   completionHandler:);
  if ([self.UIDelegate respondsToSelector:confirmationSelector]) {
    [self.UIDelegate webController:self
        runJavaScriptConfirmPanelWithMessage:message
                                  requestURL:net::GURLWithNSURL(
                                                 frame.request.URL)
                           completionHandler:completionHandler];
  } else {
    [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_CONFIRM
                   initiatedByFrame:frame
                            message:message
                        defaultText:nil
                         completion:^(BOOL success, NSString*) {
                           if (completionHandler) {
                             completionHandler(success);
                           }
                         }];
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                         initiatedByFrame:(WKFrameInfo*)frame
                        completionHandler:
                            (void (^)(NSString* result))completionHandler {
  GURL origin(web::GURLOriginWithWKSecurityOrigin(frame.securityOrigin));
  if (web::GetWebClient()->IsAppSpecificURL(origin) && _webUIManager) {
    std::string mojoResponse =
        self.mojoFacade->HandleMojoMessage(base::SysNSStringToUTF8(prompt));
    completionHandler(base::SysUTF8ToNSString(mojoResponse));
    return;
  }

  if (self.shouldSuppressDialogs) {
    [self didSuppressDialog];
    completionHandler(nil);
    return;
  }

  SEL textInputSelector = @selector(webController:
            runJavaScriptTextInputPanelWithPrompt:
                                      defaultText:
                                       requestURL:
                                completionHandler:);
  if ([self.UIDelegate respondsToSelector:textInputSelector]) {
    GURL requestURL = net::GURLWithNSURL(frame.request.URL);
    [self.UIDelegate webController:self
        runJavaScriptTextInputPanelWithPrompt:prompt
                                  defaultText:defaultText
                                   requestURL:requestURL
                            completionHandler:completionHandler];
  } else {
    [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_PROMPT
                   initiatedByFrame:frame
                            message:prompt
                        defaultText:defaultText
                         completion:^(BOOL, NSString* input) {
                           if (completionHandler) {
                             completionHandler(input);
                           }
                         }];
  }
}

#pragma mark -
#pragma mark WKNavigationDelegate Methods

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  _webProcessIsDead = NO;
  if (_isBeingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  // The page will not be changed until this navigation is commited, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationAction:action];

  // Invalid URLs should not be loaded.  However, simply doing nothing upon
  // tapping a link or button is a jarring user experience.  Instead, cancel
  // the invalid navigation and load about:blank.
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  if (!requestURL.is_valid()) {
    decisionHandler(WKNavigationActionPolicyCancel);
    GURL aboutBlankURL(url::kAboutBlankURL);
    web::NavigationManager::WebLoadParams loadParams(aboutBlankURL);
    loadParams.referrer = [self currentReferrer];
    self.webState->GetNavigationManager()->LoadURLWithParams(loadParams);
    return;
  }

  BOOL allowLoad = [self shouldAllowLoadWithNavigationAction:action];

  if (allowLoad) {
    allowLoad = self.webStateImpl->ShouldAllowRequest(action.request);
    if (!allowLoad && action.targetFrame.mainFrame) {
      [_pendingNavigationInfo setCancelled:YES];
    }
  }

  decisionHandler(allowLoad ? WKNavigationActionPolicyAllow
                            : WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)navigationResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  if ([navigationResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    // Create HTTP headers from the response.
    // TODO(crbug.com/546157): Due to the limited interface of
    // NSHTTPURLResponse, some data in the HttpResponseHeaders generated here is
    // inexact.  Once UIWebView is no longer supported, update WebState's
    // implementation so that the Content-Language and the MIME type can be set
    // without using this imperfect conversion.
    scoped_refptr<net::HttpResponseHeaders> HTTPHeaders =
        net::CreateHeadersFromNSHTTPURLResponse(
            static_cast<NSHTTPURLResponse*>(navigationResponse.response));
    self.webStateImpl->OnHttpResponseHeadersReceived(
        HTTPHeaders.get(), net::GURLWithNSURL(navigationResponse.response.URL));
  }

  // The page will not be changed until this navigation is commited, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationResponse:navigationResponse];

  BOOL allowNavigation = navigationResponse.canShowMIMEType;
  if (allowNavigation) {
    allowNavigation =
        self.webStateImpl->ShouldAllowResponse(navigationResponse.response);
    if (!allowNavigation && navigationResponse.isForMainFrame) {
      [_pendingNavigationInfo setCancelled:YES];
    }
  }
  if ([self.passKitDownloader
          isMIMETypePassKitType:[_pendingNavigationInfo MIMEType]]) {
    GURL URL = net::GURLWithNSURL(navigationResponse.response.URL);
    [self.passKitDownloader downloadPassKitFileWithURL:URL];
  }

  handler(allowNavigation ? WKNavigationResponsePolicyAllow
                          : WKNavigationResponsePolicyCancel);
}

// TODO(stuartmorgan): Move all the guesswork around these states out of the
// superclass, and wire these up to the remaining methods.
- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  if (webViewURL.is_empty()) {
    // May happen on iOS9, however in didCommitNavigation: callback the URL
    // will be "about:blank".
    webViewURL = GURL(url::kAboutBlankURL);
  }

  // Intercept renderer-initiated navigations. If this navigation has not yet
  // been registered, do so. loadPhase check is necessary because
  // lastRegisteredRequestURL may be the same as the webViewURL on a new tab
  // created by window.open (default is about::blank).
  if (_lastRegisteredRequestURL != webViewURL ||
      self.loadPhase != web::LOAD_REQUESTED) {
    // Reset current WebUI if one exists.
    [self clearWebUI];
    // Restart app specific URL loads to properly capture state.
    // TODO(crbug.com/546347): Extract necessary tasks for app specific URL
    // navigation rather than restarting the load.
    if (web::GetWebClient()->IsAppSpecificURL(webViewURL)) {
      // Renderer-initiated loads of WebUI can be done only from other WebUI
      // pages. WebUI pages may have increased power and using the same web
      // process (which may potentially be controller by an attacker) is
      // dangerous.
      if (web::GetWebClient()->IsAppSpecificURL(_documentURL)) {
        [self abortLoad];
        NavigationManager::WebLoadParams params(webViewURL);
        [self loadWithParams:params];
      }
      return;
    } else {
      [self registerLoadRequest:webViewURL];
    }
  }
  // Ensure the URL is registered and loadPhase is as expected.
  DCHECK(_lastRegisteredRequestURL == webViewURL);
  DCHECK(self.loadPhase == web::LOAD_REQUESTED);
  _latestWKNavigation.reset([navigation retain]);
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self registerLoadRequest:net::GURLWithNSURL(webView.URL)
                   referrer:[self currentReferrer]
                 transition:ui::PAGE_TRANSITION_SERVER_REDIRECT];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  // Ignore provisional navigation failure if a new navigation has been started,
  // for example, if a page is reloaded after the start of the provisional
  // load but before the load has been committed.
  if (![_latestWKNavigation isEqual:navigation]) {
    return;
  }

  // TODO(crbug.com/570699): Remove this workaround once |stopLoading| does not
  // discard pending navigation items.
  if ((!self.webStateImpl ||
       !self.webStateImpl->GetNavigationManagerImpl().GetVisibleItem()) &&
      [error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
    // App is going to crash in this state (crbug.com/565457). Crash will occur
    // on dereferencing visible navigation item, which is null. This scenario is
    // possible after pending load was stopped for a child window. Early return
    // to prevent the crash and report UMA metric to check if crash happening
    // because the load was stopped.
    UMA_HISTOGRAM_BOOLEAN(
        "WebController.EmptyNavigationManagerCausedByStopLoading",
        [_stoppedWKNavigation isEqual:navigation]);
    return;
  }

  // Handle load cancellation for directly cancelled navigations without
  // handling their potential errors. Otherwise, handle the error.
  if ([_pendingNavigationInfo cancelled]) {
    [self loadCancelled];
  } else {
    error = WKWebViewErrorWithSource(error, PROVISIONAL_LOAD);

    if (web::IsWKWebViewSSLCertError(error))
      [self handleSSLCertError:error];
    else
      [self handleLoadError:error inMainFrame:YES];
  }

  // This must be reset at the end, since code above may need information about
  // the pending load.
  _pendingNavigationInfo.reset();
  _certVerificationErrors->Clear();
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  DCHECK_EQ(_webView, webView);
  _certVerificationErrors->Clear();
  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  [self injectWindowID];

  // This is the point where the document's URL has actually changed, and
  // pending navigation information should be applied to state information.
  [self setDocumentURL:net::GURLWithNSURL([_webView URL])];

  if (!_lastRegisteredRequestURL.is_valid() &&
      _documentURL != _lastRegisteredRequestURL) {
    // if |_lastRegisteredRequestURL| is an invalid URL, then |_documentURL|
    // will be "about:blank".
    [[self sessionController] updatePendingEntry:_documentURL];
  }
  DCHECK(_documentURL == _lastRegisteredRequestURL ||
         (!_lastRegisteredRequestURL.is_valid() &&
          _documentURL.spec() == url::kAboutBlankURL));

  self.webStateImpl->OnNavigationCommitted(_documentURL);
  [self commitPendingNavigationInfo];
  if ([self currentBackForwardListItemHolder]->navigation_type() ==
      WKNavigationTypeBackForward) {
    // A fast back/forward won't call decidePolicyForNavigationResponse, so
    // the MIME type needs to be updated explicitly.
    NSString* storedMIMEType =
        [self currentBackForwardListItemHolder]->mime_type();
    if (storedMIMEType) {
      self.webStateImpl->SetContentsMimeType(
          base::SysNSStringToUTF8(storedMIMEType));
    }
  }
  [self webPageChanged];

  [self updateSSLStatusForCurrentNavigationItem];

  // Attempt to update the HTML5 history state.
  [self updateHTML5HistoryState];

  // Report cases where SSL cert is missing for a secure connection.
  if (_documentURL.SchemeIsCryptographic()) {
    scoped_refptr<net::X509Certificate> cert =
        web::CreateCertFromChain([_webView certificateChain]);
    UMA_HISTOGRAM_BOOLEAN("WebController.WKWebViewHasCertForSecureConnection",
                          static_cast<bool>(cert));
  }
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  DCHECK(!_isHalted);
  // Trigger JavaScript driven post-document-load-completion tasks.
  // TODO(crbug.com/546350): Investigate using
  // WKUserScriptInjectionTimeAtDocumentEnd to inject this material at the
  // appropriate time rather than invoking here.
  web::ExecuteJavaScript(webView, @"__gCrWeb.didFinishNavigation()", nil);
  [self didFinishNavigation];
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self handleLoadError:WKWebViewErrorWithSource(error, NAVIGATION)
            inMainFrame:YES];
  _certVerificationErrors->Clear();
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  NSString* authMethod = challenge.protectionSpace.authenticationMethod;
  if ([authMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [authMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [authMethod isEqual:NSURLAuthenticationMethodHTTPDigest]) {
    [self handleHTTPAuthForChallenge:challenge
                   completionHandler:completionHandler];
    return;
  }

  if (![authMethod isEqual:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  base::ScopedCFTypeRef<SecTrustRef> scopedTrust(trust,
                                                 base::scoped_policy::RETAIN);
  base::WeakNSObject<CRWWebController> weakSelf(self);
  [_certVerificationController
      decideLoadPolicyForTrust:scopedTrust
                          host:challenge.protectionSpace.host
             completionHandler:^(web::CertAcceptPolicy policy,
                                 net::CertStatus status) {
               base::scoped_nsobject<CRWWebController> strongSelf(
                   [weakSelf retain]);
               if (!strongSelf) {
                 completionHandler(
                     NSURLSessionAuthChallengeRejectProtectionSpace, nil);
                 return;
               }
               [strongSelf processAuthChallenge:challenge
                            forCertAcceptPolicy:policy
                                     certStatus:status
                              completionHandler:completionHandler];
             }];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  _certVerificationErrors->Clear();
  [self webViewWebProcessDidCrash];
}

#pragma mark -
#pragma mark CRWSSLStatusUpdater DataSource/Delegate Methods

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                      host:(NSString*)host
         completionHandler:(StatusQueryHandler)completionHandler {
  [_certVerificationController querySSLStatusForTrust:std::move(trust)
                                                 host:host
                                    completionHandler:completionHandler];
}

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navigationItem {
  web::NavigationItem* currentNavigationItem =
      self.webState->GetNavigationManager()->GetLastCommittedItem();
  if (navigationItem == currentNavigationItem) {
    [self didUpdateSSLStatusForCurrentNavigationItem];
  }
}

#pragma mark -
#pragma mark KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSString* dispatcherSelectorName = self.WKWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName)
    [self performSelector:NSSelectorFromString(dispatcherSelectorName)];
}

- (void)webViewEstimatedProgressDidChange {
  if (!_isBeingDestroyed) {
    self.webStateImpl->SendChangeLoadProgress([_webView estimatedProgress]);
  }
}

- (void)webViewSecurityFeaturesDidChange {
  if (self.loadPhase == web::LOAD_REQUESTED) {
    // Do not update SSL Status for pending load. It will be updated in
    // |webView:didCommitNavigation:| callback.
    return;
  }
  [self updateSSLStatusForCurrentNavigationItem];
}

- (void)webViewLoadingStateDidChange {
  if ([_webView isLoading]) {
    [self addActivityIndicatorTask];
    return;
  }

  [self clearActivityIndicatorTasks];
  if (![self isCurrentNavigationBackForward]) {
    return;
  }

  GURL webViewURL = net::GURLWithNSURL([_webView URL]);

  // For failed navigations, WKWebView will sometimes revert to the previous URL
  // before committing the current navigation or resetting the web view's
  // |isLoading| property to NO.  If this is the first navigation for the web
  // view, this will result in an empty URL.
  BOOL navigationWasCommitted = _loadPhase != web::LOAD_REQUESTED;
  if (!navigationWasCommitted &&
      (webViewURL.is_empty() || webViewURL == _documentURL)) {
    return;
  }

  if (!navigationWasCommitted && ![_pendingNavigationInfo cancelled]) {
    // A fast back/forward within the same origin does not call
    // |didCommitNavigation:|, so signal page change explicitly.
    DCHECK_EQ(_documentURL.GetOrigin(), webViewURL.GetOrigin());
    [self setDocumentURL:webViewURL];
    [self webPageChanged];
  }

  [self updateSSLStatusForCurrentNavigationItem];

  // Fast back forward navigation may not call |didFinishNavigation:|, so
  // signal did finish navigation explicitly.
  if (_lastRegisteredRequestURL == _documentURL) {
    [self didFinishNavigation];
  }
}

- (void)webViewTitleDidChange {
  // WKWebView's title becomes empty when the web process dies; ignore that
  // update.
  if (_webProcessIsDead) {
    DCHECK_EQ([_webView title].length, 0U);
    return;
  }

  if ([self.delegate
          respondsToSelector:@selector(webController:titleDidChange:)]) {
    DCHECK([_webView title]);
    [self.delegate webController:self titleDidChange:[_webView title]];
  }
}

- (void)webViewURLDidChange {
  // TODO(stuartmorgan): Determine if there are any cases where this still
  // happens, and if so whether anything should be done when it does.
  if (![_webView URL]) {
    DVLOG(1) << "Received nil URL callback";
    return;
  }
  GURL URL(net::GURLWithNSURL([_webView URL]));
  // URL changes happen at three points:
  // 1) When a load starts; at this point, the load is provisional, and
  //    it should be ignored until it's committed, since the document/window
  //    objects haven't changed yet.
  // 2) When a non-document-changing URL change happens (hash change,
  //    history.pushState, etc.). This URL change happens instantly, so should
  //    be reported.
  // 3) When a navigation error occurs after provisional navigation starts,
  //    the URL reverts to the previous URL without triggering a new navigation.
  //
  // If |isLoading| is NO, then it must be case 2 or 3. If the last committed
  // URL (_documentURL) matches the current URL, assume that it is a revert from
  // navigation failure and do nothing. If the URL does not match, assume it is
  // a non-document-changing URL change, and handle accordingly.
  //
  // If |isLoading| is YES, then it could either be case 1, or it could be case
  // 2 on a page that hasn't finished loading yet. If it's possible that it
  // could be a same-page navigation (in which case there may not be any other
  // callback about the URL having changed), then check the actual page URL via
  // JavaScript. If the origin of the new URL matches the last committed URL,
  // then check window.location.href, and if it matches, trust it. The origin
  // check ensures that if a site somehow corrupts window.location.href it can't
  // do a redirect to a slow-loading target page while it is still loading to
  // spoof the origin. On a document-changing URL change, the
  // window.location.href will match the previous URL at this stage, not the web
  // view's current URL.
  if (![_webView isLoading]) {
    if (_documentURL == URL)
      return;
    [self URLDidChangeWithoutDocumentChange:URL];
  } else if ([self isKVOChangePotentialSameDocumentNavigationToURL:URL]) {
    [_webView
        evaluateJavaScript:@"window.location.href"
         completionHandler:^(id result, NSError* error) {
           // If the web view has gone away, or the location
           // couldn't be retrieved, abort.
           if (!_webView || ![result isKindOfClass:[NSString class]]) {
             return;
           }
           GURL JSURL([result UTF8String]);
           // Check that window.location matches the new URL. If
           // it does not, this is a document-changing URL change as
           // the window location would not have changed to the new
           // URL when the script was called.
           BOOL windowLocationMatchesNewURL = JSURL == URL;
           // Re-check origin in case navigaton has occured since
           // start of JavaScript evaluation.
           BOOL newURLOriginMatchesDocumentURLOrigin =
               _documentURL.GetOrigin() == URL.GetOrigin();
           // Check that the web view URL still matches the new URL.
           // TODO(crbug.com/563568): webViewURLMatchesNewURL check
           // may drop same document URL changes if pending URL
           // change occurs immediately after. Revisit heuristics to
           // prevent this.
           BOOL webViewURLMatchesNewURL =
               net::GURLWithNSURL([_webView URL]) == URL;
           // Check that the new URL is different from the current
           // document URL. If not, URL change should not be reported.
           BOOL URLDidChangeFromDocumentURL = URL != _documentURL;
           if (windowLocationMatchesNewURL &&
               newURLOriginMatchesDocumentURLOrigin &&
               webViewURLMatchesNewURL && URLDidChangeFromDocumentURL) {
             [self URLDidChangeWithoutDocumentChange:URL];
           }
         }];
  }
}

- (BOOL)isKVOChangePotentialSameDocumentNavigationToURL:(const GURL&)newURL {
  DCHECK([_webView isLoading]);
  // If the origin changes, it can't be same-document.
  if (_documentURL.GetOrigin().is_empty() ||
      _documentURL.GetOrigin() != newURL.GetOrigin()) {
    return NO;
  }
  if (self.loadPhase == web::LOAD_REQUESTED) {
    // Normally LOAD_REQUESTED indicates that this is a regular, pending
    // navigation, but it can also happen during a fast-back navigation across
    // a hash change, so that case is potentially a same-document navigation.
    return web::GURLByRemovingRefFromGURL(newURL) ==
           web::GURLByRemovingRefFromGURL(_documentURL);
  }
  // If it passes all the checks above, it might be (but there's no guarantee
  // that it is).
  return YES;
}

- (void)URLDidChangeWithoutDocumentChange:(const GURL&)newURL {
  DCHECK(newURL == net::GURLWithNSURL([_webView URL]));
  DCHECK_EQ(_documentURL.host(), newURL.host());
  DCHECK(_documentURL != newURL);

  // If called during window.history.pushState or window.history.replaceState
  // JavaScript evaluation, only update the document URL. This callback does not
  // have any information about the state object and cannot create (or edit) the
  // navigation entry for this page change. Web controller will sync with
  // history changes when a window.history.didPushState or
  // window.history.didReplaceState message is received, which should happen in
  // the next runloop.
  //
  // Otherwise, simulate the whole delegate flow for a load (since the
  // superclass currently doesn't have a clean separation between URL changes
  // and document changes). Note that the order of these calls is important:
  // registering a load request logically comes before updating the document
  // URL, but also must come first since it uses state that is reset on URL
  // changes.
  if (!_changingHistoryState) {
    // If this wasn't a previously-expected load (e.g., certain back/forward
    // navigations), register the load request.
    if (![self isLoadRequestPendingForURL:newURL])
      [self registerLoadRequest:newURL];
  }

  [self setDocumentURL:newURL];

  if (!_changingHistoryState) {
    [self didStartLoadingURL:_documentURL updateHistory:YES];
    [self updateSSLStatusForCurrentNavigationItem];
    [self didFinishNavigation];
  }
}

- (BOOL)isLoadRequestPendingForURL:(const GURL&)targetURL {
  if (self.loadPhase != web::LOAD_REQUESTED)
    return NO;

  web::NavigationItem* pendingItem =
      self.webState->GetNavigationManager()->GetPendingItem();
  return pendingItem && pendingItem->GetURL() == targetURL;
}

- (void)loadRequestForCurrentNavigationItem {
  DCHECK(_webView && !self.nativeController);
  DCHECK([self currentSessionEntry]);
  // If a load is kicked off on a WKWebView with a frame whose size is {0, 0} or
  // that has a negative dimension for a size, rendering issues occur that
  // manifest in erroneous scrolling and tap handling (crbug.com/574996,
  // crbug.com/577793).
  DCHECK_GT(CGRectGetWidth([_webView frame]), 0.0);
  DCHECK_GT(CGRectGetHeight([_webView frame]), 0.0);

  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];
  BOOL isFormPOSTResubmission =
      [holder->http_method() isEqual:@"POST"] &&
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem =
      [self currentSessionEntry].navigationItemImpl;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  // If the request has POST data and is not a form resubmission, configure and
  // run the POST request.
  if (POSTData.length && !isFormPOSTResubmission) {
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:[self currentHTTPHeaders]];
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    [self loadPOSTRequest:request];
    return;
  }

  ProceduralBlock defaultNavigationBlock = ^{
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    [self loadRequest:request];
  };

  // When navigating via WKBackForwardListItem to pages created or updated by
  // calls to pushState() and replaceState(), sometimes core.js is not injected
  // correctly.  This means that calling window.history navigation functions
  // will invoke WKWebView's non-overridden implementations, causing a mismatch
  // between the WKBackForwardList and NavigationManager.
  // TODO(crbug.com/659816): Figure out how to prevent core.js injection flake.
  if (currentItem->HasStateBeenReplaced() ||
      currentItem->IsCreatedFromPushState()) {
    defaultNavigationBlock();
    return;
  }

  // If there is no corresponding WKBackForwardListItem, or the item is not in
  // the current WKWebView's back-forward list, navigating using WKWebView API
  // is not possible. In this case, fall back to the default navigation
  // mechanism.
  if (!holder->back_forward_list_item() ||
      ![self isBackForwardListItemValid:holder->back_forward_list_item()]) {
    defaultNavigationBlock();
    return;
  }

  ProceduralBlock webViewNavigationBlock = ^{
    // If the current navigation URL is the same as the URL of the visible
    // page, that means the user requested a reload. |goToBackForwardListItem|
    // will be a no-op when it is passed the current back forward list item,
    // so |reload| must be explicitly called.
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    if ([self currentNavigationURL] == net::GURLWithNSURL([_webView URL])) {
      [_webView reload];
    } else {
      // |didCommitNavigation:| may not be called for fast navigation, so update
      // the navigation type now as it is already known.
      holder->set_navigation_type(WKNavigationTypeBackForward);
      [_webView goToBackForwardListItem:holder->back_forward_list_item()];
    }
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.

  if (!isFormPOSTResubmission ||
      currentItem->ShouldSkipResubmitDataConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  DCHECK(isFormPOSTResubmission);
  [self.delegate webController:self
      onFormResubmissionForRequest:nil
                     continueBlock:webViewNavigationBlock
                       cancelBlock:defaultNavigationBlock];
}

#pragma mark -
#pragma mark Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  [self removeWebViewAllowingCachedReconstruction:NO];

  _lastRegisteredRequestURL = _defaultURL;
  [_containerView displayWebViewContentView:webViewContentView];
  [self setWebView:static_cast<WKWebView*>(webViewContentView.webView)];
}

- (void)resetInjectedWebViewContentView {
  [self setWebView:nil];
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

- (void)simulateLoadRequestWithURL:(const GURL&)URL {
  _lastRegisteredRequestURL = URL;
  _loadPhase = web::LOAD_REQUESTED;
}

- (NSString*)externalRequestWindowName {
  if (!_externalRequest || !_externalRequest->window_name)
    return @"";
  return _externalRequest->window_name;
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

- (NSString*)refererFromNavigationAction:(WKNavigationAction*)action {
  return [action.request valueForHTTPHeaderField:@"Referer"];
}

@end
