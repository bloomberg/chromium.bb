// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/net/http_response_headers_util.h"
#import "ios/web/crw_network_activity_indicator_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/web_load_params.h"
#import "ios/web/net/crw_cert_verification_controller.h"
#include "ios/web/public/cert_store.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/ui_web_view_util.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/error_translation_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/crw_wk_web_view_crash_detector.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "ios/web/webui/crw_web_ui_manager.h"
#include "net/cert/x509_certificate.h"
#import "net/base/mac/url_conversions.h"
#include "net/ssl/ssl_info.h"
#include "url/url_constants.h"

namespace {
// Extracts Referer value from WKNavigationAction request header.
NSString* GetRefererFromNavigationAction(WKNavigationAction* action) {
  return [action.request valueForHTTPHeaderField:@"Referer"];
}

NSString* const kScriptMessageName = @"crwebinvoke";
NSString* const kScriptImmediateName = @"crwebinvokeimmediate";

// Utility functions for storing the source of NSErrors received by WKWebViews:
// - Errors received by |-webView:didFailProvisionalNavigation:withError:| are
//   recorded using WKWebViewErrorSource::PROVISIONAL_LOAD.  These should be
//   aborted.
// - Errors received by |-webView:didFailNavigation:withError:| are recorded
//   using WKWebViewsource::NAVIGATION.  These errors should not be aborted, as
//   the WKWebView will automatically retry the load.
NSString* const kWKWebViewErrorSourceKey = @"ErrorSource";
typedef enum { NONE = 0, PROVISIONAL_LOAD, NAVIGATION } WKWebViewErrorSource;
NSError* WKWebViewErrorWithSource(NSError* error, WKWebViewErrorSource source) {
  DCHECK(error);
  base::scoped_nsobject<NSMutableDictionary> userInfo(
      [error.userInfo mutableCopy]);
  [userInfo setObject:@(source) forKey:kWKWebViewErrorSourceKey];
  return [NSError errorWithDomain:error.domain
                             code:error.code
                         userInfo:userInfo];
}
WKWebViewErrorSource WKWebViewErrorSourceFromError(NSError* error) {
  DCHECK(error);
  return static_cast<WKWebViewErrorSource>(
      [error.userInfo[kWKWebViewErrorSourceKey] integerValue]);
}

}  // namespace

@interface CRWWKWebViewWebController ()<WKNavigationDelegate, WKUIDelegate> {
  // The WKWebView managed by this instance.
  base::scoped_nsobject<WKWebView> _wkWebView;

  // The Watch Dog that detects and reports WKWebView crashes.
  base::scoped_nsobject<CRWWKWebViewCrashDetector> _crashDetector;

  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(stuartmorgan): Remove this in favor of just updating the session
  // controller and treating that as authoritative. For now, this allows sharing
  // the flow that's currently in the superclass.
  GURL _documentURL;

  // A set of script managers whose scripts have been injected into the current
  // page.
  // TODO(stuartmorgan): Revisit this approach; it's intended only as a stopgap
  // measure to make all the existing script managers work. Longer term, there
  // should probably be a couple of points where managers can register to have
  // things happen automatically based on page lifecycle, and if they don't want
  // to use one of those fixed points, they should make their scripts internally
  // idempotent.
  base::scoped_nsobject<NSMutableSet> _injectedScriptManagers;

  // Referrer pending for the next navigated page. Lifetime of this value starts
  // at |decidePolicyForNavigationAction| where the referrer is extracted from
  // the request and ends at |didCommitNavigation| where the request is
  // committed.
  base::scoped_nsobject<NSString> _pendingReferrerString;

  // Referrer for the current page.
  base::scoped_nsobject<NSString> _currentReferrerString;

  // Backs the property of the same name.
  base::scoped_nsobject<NSString> _documentMIMEType;

  // Navigation type of the pending navigation action of the main frame. This
  // value is assigned at |decidePolicyForNavigationAction| where the navigation
  // type is extracted from the request and associated with a committed
  // navigation item at |didCommitNavigation|.
  scoped_ptr<WKNavigationType> _pendingNavigationTypeForMainFrame;

  // Whether the web page is currently performing window.history.pushState or
  // window.history.replaceState
  // Set to YES on window.history.willChangeState message. To NO on
  // window.history.didPushState or window.history.didReplaceState.
  BOOL _changingHistoryState;

  // CRWWebUIManager object for loading WebUI pages.
  base::scoped_nsobject<CRWWebUIManager> _webUIManager;

  // Controller used for certs verification to help with blocking requests with
  // bad SSL cert, presenting SSL interstitials and determining SSL status for
  // Navigation Items.
  base::scoped_nsobject<CRWCertVerificationController>
      _certVerificationController;

  // Whether the pending navigation has been directly cancelled in
  // |decidePolicyForNavigationAction| or |decidePolicyForNavigationResponse|.
  // Cancelled navigations should be simply discarded without handling any
  // specific error.
  BOOL _pendingNavigationCancelled;
}

// Response's MIME type of the last known navigation.
@property(nonatomic, copy) NSString* documentMIMEType;

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(nonatomic, readonly) NSDictionary* wkWebViewObservers;

// Returns the string to use as the request group ID in the user agent. Returns
// nil unless the network stack is enabled.
@property(nonatomic, readonly) NSString* requestGroupIDForUserAgent;

// Activity indicator group ID for this web controller.
@property(nonatomic, readonly) NSString* activityIndicatorGroupID;

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Identifier used for storing and retrieving certificates.
@property(nonatomic, readonly) int certGroupID;
#endif  // #if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

// Returns the WKWebViewConfigurationProvider associated with the web
// controller's BrowserState.
- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider;

// Creates a web view with given |config|. No-op if web view is already created.
- (void)ensureWebViewCreatedWithConfiguration:(WKWebViewConfiguration*)config;

// Returns a new autoreleased web view created with given configuration.
- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config;

// Sets the value of the webView property, and performs its basic setup.
- (void)setWebView:(WKWebView*)webView;

// Returns whether the given navigation is triggered by a user link click.
- (BOOL)isLinkNavigation:(WKNavigationType)navigationType;

// Sets value of the pendingReferrerString property.
- (void)setPendingReferrerString:(NSString*)referrer;

// Extracts the referrer value from WKNavigationAction and sets it as a pending.
// The referrer is known in |decidePolicyForNavigationAction| however it must
// be in a pending state until |didCommitNavigation| where it becames current.
- (void)updatePendingReferrerFromNavigationAction:(WKNavigationAction*)action;

// Replaces the current referrer with the pending one. Referrer becames current
// at |didCommitNavigation| callback.
- (void)commitPendingReferrerString;

// Discards the pending referrer.
- (void)discardPendingReferrerString;

// Extracts the current navigation type from WKNavigationAction and sets it as
// the pending navigation type. The value should be considered pending until it
// becomes associated with a navigation item at |didCommitNavigation|.
- (void)updatePendingNavigationTypeForMainFrameFromNavigationAction:
    (WKNavigationAction*)action;

// Discards the pending navigation type.
- (void)discardPendingNavigationTypeForMainFrame;

// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder;

// Stores the current WKBackForwardListItem and the current navigation type
// with the current navigation item.
- (void)updateCurrentBackForwardListItemHolder;

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;

// Returns a new CRWWKWebViewCrashDetector created with the given |webView| or
// nil if |webView| is nil. Callers are responsible for releasing the object.
- (CRWWKWebViewCrashDetector*)newCrashDetectorWithWebView:(WKWebView*)webView;

// Called when web view process has been terminated.
- (void)webViewWebProcessDidCrash;

// Asynchronously returns the referrer policy for the current page.
- (void)queryPageReferrerPolicy:(void(^)(NSString*))responseHandler;

// Informs CWRWebDelegate that CRWWebController has detected and blocked a
// popup.
- (void)didBlockPopupWithURL:(GURL)popupURL
                   sourceURL:(GURL)sourceURL
              referrerPolicy:(const std::string&)referrerPolicyString;

// Convenience method to inform CWRWebDelegate about a blocked popup.
- (void)didBlockPopupWithURL:(GURL)popupURL sourceURL:(GURL)sourceURL;

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Called when a load ends in an SSL error and certificate chain.
- (void)handleSSLCertError:(NSError*)error;
#endif

// Adds an activity indicator tasks for this web controller.
- (void)addActivityIndicatorTask;

// Clears all activity indicator tasks for this web controller.
- (void)clearActivityIndicatorTasks;

// Updates |security_style| and |cert_status| for the NavigationItem with ID
// |navigationItemID|, if URL and certificate chain still match |host| and
// |certChain|.
- (void)updateSSLStatusForNavigationItemWithID:(int)navigationItemID
                                     certChain:(NSArray*)chain
                                          host:(NSString*)host
                             withSecurityStyle:(web::SecurityStyle)style
                                    certStatus:(net::CertStatus)certStatus;

// Asynchronously obtains SSL status from given |certChain| and |host| and
// updates current navigation item. Before scheduling update changes SSLStatus'
// cert_status and security_style to default.
- (void)scheduleSSLStatusUpdateUsingCertChain:(NSArray*)chain
                                         host:(NSString*)host;

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Updates SSL status for the current navigation item based on the information
// provided by web view.
- (void)updateSSLStatusForCurrentNavigationItem;
#endif

// Registers load request with empty referrer and link or client redirect
// transition based on user interaction state.
- (void)registerLoadRequest:(const GURL&)url;

// Called when a non-document-changing URL change occurs. Updates the
// _documentURL, and informs the superclass of the change.
- (void)URLDidChangeWithoutDocumentChange:(const GURL&)URL;

// Called when web controller receives a new message from the web page.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message;

// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;

// Used to decide whether a load that generates errors with the
// NSURLErrorCancelled code should be cancelled.
- (BOOL)shouldAbortLoadForCancelledError:(NSError*)error;

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange;

// Called when WKWebView certificateChain or hasOnlySecureContent property has
// changed.
- (void)webViewSecurityFeaturesDidChange;
#endif  // !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

// Called when WKWebView loading state has been changed.
- (void)webViewLoadingStateDidChange;

// Called when WKWebView title has been changed.
- (void)webViewTitleDidChange;

// Called when WKWebView URL has been changed.
- (void)webViewURLDidChange;

@end

@implementation CRWWKWebViewWebController

#pragma mark CRWWebController public methods

- (instancetype)initWithWebState:(scoped_ptr<web::WebStateImpl>)webState {
  DCHECK(webState);
  web::BrowserState* browserState = webState->GetBrowserState();
  self = [super initWithWebState:webState.Pass()];
  if (self) {
    _certVerificationController.reset([[CRWCertVerificationController alloc]
        initWithBrowserState:browserState]);
  }
  return self;
}

- (BOOL)keyboardDisplayRequiresUserAction {
  // TODO(stuartmorgan): Find out whether YES or NO is correct; see comment
  // in protected header.
  NOTIMPLEMENTED();
  return NO;
}

- (void)setKeyboardDisplayRequiresUserAction:(BOOL)requiresUserAction {
  NOTIMPLEMENTED();
}

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  NSString* safeScript = [self scriptByAddingWindowIDCheckForScript:script];
  web::EvaluateJavaScript(_wkWebView, safeScript, handler);
}

- (web::WebViewType)webViewType {
  return web::WK_WEB_VIEW_TYPE;
}

- (void)evaluateUserJavaScript:(NSString*)script {
  [self setUserInteractionRegistered:YES];
  web::EvaluateJavaScript(_wkWebView, script, nil);
}

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
- (void)terminateNetworkActivity {
  web::CertStore::GetInstance()->RemoveCertsForGroup(self.certGroupID);
  [super terminateNetworkActivity];
}
#endif  // #if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

- (void)setPageDialogOpenPolicy:(web::PageDialogOpenPolicy)policy {
  // TODO(eugenebut): implement dialogs/windows suppression using
  // WKNavigationDelegate methods where possible.
  [super setPageDialogOpenPolicy:policy];
}

- (void)close {
  [_certVerificationController shutDown];
  [super close];
}

#pragma mark -
#pragma mark Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  [super injectWebViewContentView:webViewContentView];
  [self setWebView:static_cast<WKWebView*>(webViewContentView.webView)];
}

#pragma mark - Protected property implementations

- (UIView*)webView {
  return _wkWebView.get();
}

- (UIScrollView*)webScrollView {
  return [_wkWebView scrollView];
}

- (BOOL)ignoreURLVerificationFailures {
  return NO;
}

- (NSString*)title {
  return [_wkWebView title];
}

- (NSString*)currentReferrerString {
  return _currentReferrerString.get();
}

#pragma mark Protected method implementations

- (void)ensureWebViewCreated {
  WKWebViewConfiguration* config =
      [self webViewConfigurationProvider].GetWebViewConfiguration();
  [self ensureWebViewCreatedWithConfiguration:config];
}

- (void)resetWebView {
  [self setWebView:nil];
}

- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel);
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  return _documentURL;
}

// TODO(stuartmorgan): Remove this method and use the API for WKWebView,
// making the reset-on-each-load behavior specific to the UIWebView subclass.
- (void)registerUserAgent {
  web::BuildAndRegisterUserAgentForUIWebView([self requestGroupIDForUserAgent],
                                             [self useDesktopUserAgent]);
}

- (BOOL)isCurrentNavigationItemPOST {
  // |_pendingNavigationTypeForMainFrame| will be nil if
  // |decidePolicyForNavigationAction| was not reached.
  WKNavigationType type =
      (_pendingNavigationTypeForMainFrame)
          ? *_pendingNavigationTypeForMainFrame
          : [self currentBackForwardListItemHolder]->navigation_type();
  return type == WKNavigationTypeFormSubmitted ||
         type == WKNavigationTypeFormResubmitted;
}

// The core.js cannot pass messages back to obj-c  if it is injected
// to |WEB_VIEW_DOCUMENT| because it does not support iframe creation used
// by core.js to communicate back. That functionality is only supported
// by |WEB_VIEW_HTML_DOCUMENT|. |WEB_VIEW_DOCUMENT| is used when displaying
// non-HTML contents (e.g. PDF documents).
- (web::WebViewDocumentType)webViewDocumentType {
  // This happens during tests.
  if (!_wkWebView) {
    return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
  }

  if (!self.documentMIMEType) {
    return web::WEB_VIEW_DOCUMENT_TYPE_UNKNOWN;
  }

  if ([self.documentMIMEType isEqualToString:@"text/html"] ||
      [self.documentMIMEType isEqualToString:@"application/xhtml+xml"] ||
      [self.documentMIMEType isEqualToString:@"application/xml"]) {
    return web::WEB_VIEW_DOCUMENT_TYPE_HTML;
  }

  return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
}

- (void)loadRequest:(NSMutableURLRequest*)request {
  [_wkWebView loadRequest:request];
}

- (void)loadWebHTMLString:(NSString*)html forURL:(const GURL&)URL {
  [_wkWebView loadHTMLString:html baseURL:net::NSURLWithGURL(URL)];
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  return [_injectedScriptManagers containsObject:jsInjectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Skip evaluation if there's no content (e.g., if what's being injected is
  // an umbrella manager).
  if ([script length]) {
    [super injectScript:script forClass:JSInjectionManagerClass];
    // Every injection except windowID requires windowID check.
    if (JSInjectionManagerClass != [CRWJSWindowIdManager class])
      script = [self scriptByAddingWindowIDCheckForScript:script];
    web::EvaluateJavaScript(_wkWebView, script, nil);
  }
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

- (void)willLoadCurrentURLInWebView {
  // TODO(stuartmorgan): Get a WKWebView version of the request ID verification
  // code working for debug builds.
}

- (void)loadRequestForCurrentNavigationItem {
  DCHECK(self.webView && !self.nativeController);

  ProceduralBlock defaultNavigationBlock = ^{
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    [self loadRequest:[self requestForCurrentNavigationItem]];
  };

  // If there is no corresponding WKBackForwardListItem, or the item is not in
  // the current WKWebView's back-forward list, navigating using WKWebView API
  // is not possible. In this case, fall back to the default navigation
  // mechanism.
  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];
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
    if ([self currentNavigationURL] == net::GURLWithNSURL([_wkWebView URL])) {
      [_wkWebView reload];
    } else {
      [_wkWebView goToBackForwardListItem:holder->back_forward_list_item()];
    }
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.
  web::NavigationItemImpl* currentItem =
      [self currentSessionEntry].navigationItemImpl;
  if ((holder->navigation_type() != WKNavigationTypeFormResubmitted &&
       holder->navigation_type() != WKNavigationTypeFormSubmitted) ||
      currentItem->ShouldSkipResubmitDataConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  [self.delegate webController:self
      onFormResubmissionForRequest:nil
                     continueBlock:webViewNavigationBlock
                       cancelBlock:defaultNavigationBlock];
}

// Overrides the hashchange workaround in the super class that manually
// triggers Javascript hashchange events. If navigating with native API,
// i.e. using a back forward list item, hashchange events will be triggered
// automatically, so no URL tampering is required.
- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem {
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(toItem);

  if (holder->back_forward_list_item()) {
    return toItem->GetURL();
  }
  return [super URLForHistoryNavigationFromItem:fromItem toItem:toItem];
}

- (void)setPageChangeProbability:(web::PageChangeProbability)probability {
  // Nothing to do; no polling timer.
}

- (void)abortWebLoad {
  [_wkWebView stopLoading];
}

- (void)resetLoadState {
  // Nothing to do.
}

- (void)setSuppressDialogsWithHelperScript:(NSString*)script {
  [self evaluateJavaScript:script stringResultHandler:nil];
}

- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState {
  // After rendering a web page, WKWebView keeps the |minimumZoomScale| and
  // |maximumZoomScale| properties of its scroll view constant while adjusting
  // the |zoomScale| property accordingly.  The maximum-scale or minimum-scale
  // meta tags of a page may have changed since the state was recorded, so clamp
  // the zoom scale to the current range if necessary.
  DCHECK(zoomState.IsValid());
  // Legacy-format scroll states cannot be applied to WKWebViews.
  if (zoomState.IsLegacyFormat())
    return;
  CGFloat zoomScale = zoomState.zoom_scale();
  if (zoomScale < self.webScrollView.minimumZoomScale)
    zoomScale = self.webScrollView.minimumZoomScale;
  if (zoomScale > self.webScrollView.maximumZoomScale)
    zoomScale = self.webScrollView.maximumZoomScale;
  self.webScrollView.zoomScale = zoomScale;
}

- (void)handleCancelledError:(NSError*)error {
  if ([self shouldAbortLoadForCancelledError:error]) {
    // Do not abort the load for WKWebView, because calling stopLoading may
    // stop the subsequent provisional load as well.
    [self loadCancelled];
    [[self sessionController] discardNonCommittedEntries];
  }
}

#pragma mark Private methods

- (NSString*)documentMIMEType {
  return _documentMIMEType.get();
}

- (void)setDocumentMIMEType:(NSString*)type {
  _documentMIMEType.reset([type copy]);
}

- (NSDictionary*)wkWebViewObservers {
  return @{
#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"certificateChain" : @"webViewSecurityFeaturesDidChange",
    @"hasOnlySecureContent" : @"webViewSecurityFeaturesDidChange",
#endif
    @"loading" : @"webViewLoadingStateDidChange",
    @"title" : @"webViewTitleDidChange",
    @"URL" : @"webViewURLDidChange",
  };
}

- (NSString*)requestGroupIDForUserAgent {
#if defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  return self.webStateImpl->GetRequestGroupID();
#else
  return nil;
#endif
}

- (NSString*)activityIndicatorGroupID {
  return [NSString stringWithFormat:
      @"WKWebViewWebController.NetworkActivityIndicatorKey.%@",
          self.webStateImpl->GetRequestGroupID()];
}

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
- (int)certGroupID {
  DCHECK(self.webStateImpl);
  // Request tracker IDs are used as certificate groups.
  return self.webStateImpl->GetRequestTracker()->identifier();
}
#endif

- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider {
  DCHECK(self.webStateImpl);
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  return web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
}

- (void)ensureWebViewCreatedWithConfiguration:(WKWebViewConfiguration*)config {
  if (!_wkWebView) {
    [self setWebView:[self createWebViewWithConfiguration:config]];
    // Notify super class about created web view. -webViewDidChange is not
    // called from -setWebView:scriptMessageRouter: as the latter used in unit
    // tests with fake web view, which cannot be added to view hierarchy.
    [self webViewDidChange];
  }
}

- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config {
  return [web::CreateWKWebView(CGRectZero, config,
                               self.webStateImpl->GetBrowserState(),
                               [self requestGroupIDForUserAgent],
                               [self useDesktopUserAgent]) autorelease];
}

- (void)setWebView:(WKWebView*)webView {
  DCHECK_NE(_wkWebView.get(), webView);

  // Unwind the old web view.
  // TODO(eugenebut): Remove CRWWKScriptMessageRouter once crbug.com/543374 is
  // fixed.
  CRWWKScriptMessageRouter* messageRouter =
      [self webViewConfigurationProvider].GetScriptMessageRouter();
  if (_wkWebView) {
    [messageRouter removeAllScriptMessageHandlersForWebView:_wkWebView];
  }
  [_wkWebView setNavigationDelegate:nil];
  [_wkWebView setUIDelegate:nil];
  for (NSString* keyPath in self.wkWebViewObservers) {
    [_wkWebView removeObserver:self forKeyPath:keyPath];
  }
  [self clearActivityIndicatorTasks];

  _wkWebView.reset([webView retain]);

  // Set up the new web view.
  if (webView) {
    base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
    void (^messageHandler)(WKScriptMessage*) = ^(WKScriptMessage* message) {
      [weakSelf didReceiveScriptMessage:message];
    };
    [messageRouter setScriptMessageHandler:messageHandler
                                      name:kScriptMessageName
                                   webView:webView];
    [messageRouter setScriptMessageHandler:messageHandler
                                      name:kScriptImmediateName
                                   webView:webView];
  }
  [_wkWebView setNavigationDelegate:self];
  [_wkWebView setUIDelegate:self];
  for (NSString* keyPath in self.wkWebViewObservers) {
    [_wkWebView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
  _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  _crashDetector.reset([self newCrashDetectorWithWebView:_wkWebView]);
  _documentURL = [self defaultURL];
}

- (BOOL)isLinkNavigation:(WKNavigationType)navigationType {
  switch (navigationType) {
    case WKNavigationTypeLinkActivated:
      return YES;
    case WKNavigationTypeOther:
      return [self userClickedRecently];
    default:
      return NO;
  }
}

- (void)setPendingReferrerString:(NSString*)referrer {
  _pendingReferrerString.reset([referrer copy]);
}

- (void)updatePendingReferrerFromNavigationAction:(WKNavigationAction*)action {
  if (action.targetFrame.mainFrame)
    [self setPendingReferrerString:GetRefererFromNavigationAction(action)];
}

- (void)commitPendingReferrerString {
  _currentReferrerString.reset(_pendingReferrerString.release());
}

- (void)discardPendingReferrerString {
  _pendingReferrerString.reset();
}

- (void)updatePendingNavigationTypeForMainFrameFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame)
    _pendingNavigationTypeForMainFrame.reset(
        new WKNavigationType(action.navigationType));
}

- (void)discardPendingNavigationTypeForMainFrame {
  _pendingNavigationTypeForMainFrame.reset();
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
  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];
  // If |decidePolicyForNavigationAction| gets called for every load,
  // it should not necessary to perform this if check - just
  // overwrite the holder with the newest data. See crbug.com/520279.
  if (_pendingNavigationTypeForMainFrame) {
    holder->set_back_forward_list_item(
        [_wkWebView backForwardList].currentItem);
    holder->set_navigation_type(*_pendingNavigationTypeForMainFrame);
    _pendingNavigationTypeForMainFrame.reset();
  }
}

- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = [_wkWebView backForwardList];
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

- (CRWWKWebViewCrashDetector*)newCrashDetectorWithWebView:(WKWebView*)webView {
  // iOS9 provides crash detection API.
  if (!webView || base::ios::IsRunningOnIOS9OrLater())
    return nil;

  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  id crashHandler = ^{
    [weakSelf webViewWebProcessDidCrash];
  };
  return [[CRWWKWebViewCrashDetector alloc] initWithWebView:webView
                                               crashHandler:crashHandler];
}

- (void)webViewWebProcessDidCrash {
  if ([self.delegate respondsToSelector:
          @selector(webControllerWebProcessDidCrash:)]) {
    [self.delegate webControllerWebProcessDidCrash:self];
  }
}

- (void)queryPageReferrerPolicy:(void(^)(NSString*))responseHandler {
  DCHECK(responseHandler);
  [self evaluateJavaScript:@"__gCrWeb.getPageReferrerPolicy()"
       stringResultHandler:^(NSString* referrer, NSError* error) {
      DCHECK_NE(error.code, WKErrorJavaScriptExceptionOccurred);
      responseHandler(!error ? referrer : nil);
  }];
}

- (void)didBlockPopupWithURL:(GURL)popupURL
                   sourceURL:(GURL)sourceURL
              referrerPolicy:(const std::string&)referrerPolicyString {
  web::ReferrerPolicy referrerPolicy =
      [self referrerPolicyFromString:referrerPolicyString];
  web::Referrer referrer(sourceURL, referrerPolicy);
  NSString* const kWindowName = @"";  // obsoleted
  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  void(^showPopupHandler)() = ^{
      // On Desktop cross-window comunication is not supported for unblocked
      // popups; so it's ok to create a new independent page.
      CRWWebController* child = [[weakSelf delegate]
          webPageOrderedOpen:popupURL
                    referrer:referrer
                  windowName:kWindowName
                inBackground:NO];
      DCHECK(!child || child.sessionController.openedByDOM);
  };

  web::BlockedPopupInfo info(popupURL, referrer, kWindowName, showPopupHandler);
  [self.delegate webController:self didBlockPopup:info];
}

- (void)didBlockPopupWithURL:(GURL)popupURL sourceURL:(GURL)sourceURL {
  if (![self.delegate respondsToSelector:
      @selector(webController:didBlockPopup:)]) {
    return;
  }

  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  dispatch_async(dispatch_get_main_queue(), ^{
      [self queryPageReferrerPolicy:^(NSString* policy) {
          [weakSelf didBlockPopupWithURL:popupURL
                               sourceURL:sourceURL
                          referrerPolicy:base::SysNSStringToUTF8(policy)];
      }];
  });
}

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
- (void)handleSSLCertError:(NSError*)error {
  DCHECK(web::IsWKWebViewSSLCertError(error));

  net::SSLInfo sslInfo;
  web::GetSSLInfoFromWKWebViewSSLCertError(error, &sslInfo);

  web::SSLStatus sslStatus;
  sslStatus.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  sslStatus.cert_status = sslInfo.cert_status;
  sslStatus.cert_id = web::CertStore::GetInstance()->StoreCert(
      sslInfo.cert.get(), self.certGroupID);

  [self.delegate presentSSLError:sslInfo
                    forSSLStatus:sslStatus
                     recoverable:NO
                        callback:nullptr];
}
#endif  // #if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

- (void)addActivityIndicatorTask {
  [[CRWNetworkActivityIndicatorManager sharedInstance]
      startNetworkTaskForGroup:[self activityIndicatorGroupID]];
}

- (void)clearActivityIndicatorTasks {
  [[CRWNetworkActivityIndicatorManager sharedInstance]
      clearNetworkTasksForGroup:[self activityIndicatorGroupID]];
}

- (void)updateSSLStatusForNavigationItemWithID:(int)navigationItemID
                                     certChain:(NSArray*)chain
                                          host:(NSString*)host
                             withSecurityStyle:(web::SecurityStyle)style
                                    certStatus:(net::CertStatus)certStatus {
  web::NavigationManager* navigationManager =
      self.webStateImpl->GetNavigationManager();

  // The searched item almost always be the last one, so walk backward rather
  // than forward.
  for (int i = navigationManager->GetEntryCount() - 1; 0 <= i; i--) {
    web::NavigationItem* item = navigationManager->GetItemAtIndex(i);
    if (item->GetUniqueID() != navigationItemID)
      continue;

    // NavigationItem's UniqueID is preserved even after redirects, so
    // checking that cert and URL match is necessary.
    scoped_refptr<net::X509Certificate> cert(web::CreateCertFromChain(chain));
    int certID =
        web::CertStore::GetInstance()->StoreCert(cert.get(), self.certGroupID);
    std::string GURLHost = base::SysNSStringToUTF8(host);
    web::SSLStatus& SSLStatus = item->GetSSL();
    if (SSLStatus.cert_id == certID && item->GetURL().host() == GURLHost) {
      web::SSLStatus previousSSLStatus = item->GetSSL();
      SSLStatus.cert_status = certStatus;
      SSLStatus.security_style = style;
      if (navigationManager->GetCurrentEntryIndex() == i &&
          !previousSSLStatus.Equals(SSLStatus)) {
        [self didUpdateSSLStatusForCurrentNavigationItem];
      }
    }
    return;
  }
}

- (void)scheduleSSLStatusUpdateUsingCertChain:(NSArray*)chain
                                         host:(NSString*)host {
  // Use Navigation Item's unique ID to locate requested item after
  // obtaining cert status asynchronously.
  int itemID = self.webStateImpl->GetNavigationManager()
                   ->GetLastCommittedItem()
                   ->GetUniqueID();

  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  void (^SSLStatusResponse)(web::SecurityStyle, net::CertStatus) =
      ^(web::SecurityStyle style, net::CertStatus certStatus) {
        base::scoped_nsobject<CRWWKWebViewWebController> strongSelf(
            [weakSelf retain]);
        if (!strongSelf || [strongSelf isBeingDestroyed]) {
          return;
        }
        [strongSelf updateSSLStatusForNavigationItemWithID:itemID
                                                 certChain:chain
                                                      host:host
                                         withSecurityStyle:style
                                                certStatus:certStatus];
      };

  [_certVerificationController
      querySSLStatusForTrust:web::CreateServerTrustFromChain(chain, host)
                        host:host
           completionHandler:SSLStatusResponse];
}

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

- (void)updateSSLStatusForCurrentNavigationItem {
  if ([self isBeingDestroyed])
    return;

  web::NavigationItem* item =
      self.webStateImpl->GetNavigationManagerImpl().GetLastCommittedItem();
  if (!item)
    return;

  web::SSLStatus previousSSLStatus = item->GetSSL();

  // Starting from iOS9 WKWebView blocks active mixed content, so if
  // |hasOnlySecureContent| returns NO it means passive content.
  item->GetSSL().content_status =
      [_wkWebView hasOnlySecureContent]
          ? web::SSLStatus::NORMAL_CONTENT
          : web::SSLStatus::DISPLAYED_INSECURE_CONTENT;

  // Try updating SSLStatus for current NavigationItem asynchronously.
  scoped_refptr<net::X509Certificate> cert;
  if (item->GetURL().SchemeIsCryptographic()) {
    NSArray* chain = [_wkWebView certificateChain];
    cert = web::CreateCertFromChain(chain);
    if (cert) {
      int oldCertID = item->GetSSL().cert_id;
      std::string oldHost = item->GetSSL().cert_status_host;
      item->GetSSL().cert_id = web::CertStore::GetInstance()->StoreCert(
          cert.get(), self.certGroupID);
      item->GetSSL().cert_status_host = _documentURL.host();
      // Only recompute the SSLStatus information if the certificate or host has
      // since changed. Host can be changed in case of redirect.
      if (oldCertID != item->GetSSL().cert_id ||
          oldHost != item->GetSSL().cert_status_host) {
        // Real SSL status is unknown, reset cert status and security style.
        // They will be asynchronously updated in
        // |scheduleSSLStatusUpdateUsingCertChain|.
        item->GetSSL().cert_status = net::CertStatus();
        item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;

        NSString* host = base::SysUTF8ToNSString(_documentURL.host());
        [self scheduleSSLStatusUpdateUsingCertChain:chain host:host];
      }
    }
  }

  if (!cert) {
    item->GetSSL().cert_id = 0;
    if (!item->GetURL().SchemeIsCryptographic()) {
      // HTTP or other non-secure connection.
      item->GetSSL().security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
    } else {
      // HTTPS, no certificate (this use-case has not been observed).
      item->GetSSL().security_style = web::SECURITY_STYLE_UNKNOWN;
    }
  }

  if (!previousSSLStatus.Equals(item->GetSSL())) {
    [self didUpdateSSLStatusForCurrentNavigationItem];
  }
}

#endif  // !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

- (void)registerLoadRequest:(const GURL&)url {
  // If load request is registered via WKWebViewWebController, assume transition
  // is link or client redirect as other transitions will already be registered
  // by web controller or delegates.
  // TODO(stuartmorgan): Remove guesswork and replace with information from
  // decidePolicyForNavigationAction:.
  ui::PageTransition transition = self.userInteractionRegistered
                                      ? ui::PAGE_TRANSITION_LINK
                                      : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  // The referrer is not known yet, and will be updated later.
  const web::Referrer emptyReferrer;
  [self registerLoadRequest:url referrer:emptyReferrer transition:transition];
}

- (void)URLDidChangeWithoutDocumentChange:(const GURL&)newURL {
  DCHECK(newURL == net::GURLWithNSURL([_wkWebView URL]));
  DCHECK_EQ(_documentURL.host(), newURL.host());
  _documentURL = newURL;
  // If called during window.history.pushState or window.history.replaceState
  // JavaScript evaluation, only update the document URL. This callback does not
  // have any information about the state object and cannot create (or edit) the
  // navigation entry for this page change. Web controller will sync with
  // history changes when a window.history.didPushState or
  // window.history.didReplaceState message is received, which should happen in
  // the next runloop.
  if (!_changingHistoryState) {
    [self registerLoadRequest:_documentURL];
    [self didStartLoadingURL:_documentURL updateHistory:YES];
    [self didFinishNavigation];
  }
}

- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  // Broken out into separate method to catch errors.
  // TODO(jyquinn): Evaluate whether this is necessary for WKWebView.
  if (![self respondToWKScriptMessage:message]) {
    DLOG(WARNING) << "Message from JS not handled due to invalid format";
  }
}

- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage {
  CHECK(scriptMessage.frameInfo.mainFrame);
  int errorCode = 0;
  std::string errorMessage;
  scoped_ptr<base::Value> inputJSONData(
      base::JSONReader::ReadAndReturnError(
          base::SysNSStringToUTF8(scriptMessage.body),
          false,
          &errorCode,
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
  if (![[self windowId] isEqualToString:base::SysUTF8ToNSString(windowID)]) {
    DLOG(WARNING) << "Message from JS ignored due to non-matching windowID: "
                  << [self windowId] << " != "
                  << base::SysUTF8ToNSString(windowID);
    return NO;
  }
  base::DictionaryValue* command = nullptr;
  if (!message->GetDictionary("crwCommand", &command)) {
    return NO;
  }
  if ([scriptMessage.name isEqualToString:kScriptImmediateName] ||
      [scriptMessage.name isEqualToString:kScriptMessageName]) {
    return [self respondToMessage:command
                userIsInteracting:[self userIsInteracting]
                        originURL:net::GURLWithNSURL([_wkWebView URL])];
  }

  NOTREACHED();
  return NO;
}

- (SEL)selectorToHandleJavaScriptCommand:(const std::string&)command {
  static std::map<std::string, SEL>* handlers = nullptr;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    handlers = new std::map<std::string, SEL>();
    (*handlers)["window.history.didPushState"] =
        @selector(handleWindowHistoryDidPushStateMessage:context:);
    (*handlers)["window.history.didReplaceState"] =
        @selector(handleWindowHistoryDidReplaceStateMessage:context:);
    (*handlers)["window.history.willChangeState"] =
        @selector(handleWindowHistoryWillChangeStateMessage:context:);
  });
  DCHECK(handlers);
  auto iter = handlers->find(command);
  return iter != handlers->end()
             ? iter->second
             : [super selectorToHandleJavaScriptCommand:command];
}

- (BOOL)shouldAbortLoadForCancelledError:(NSError*)error {
  DCHECK_EQ(error.code, NSURLErrorCancelled);
  // Do not abort the load if it is for an app specific URL, as such errors
  // are produced during the app specific URL load process.
  const GURL errorURL =
      net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
  if (web::GetWebClient()->IsAppSpecificURL(errorURL))
    return NO;
  // Don't abort NSURLErrorCancelled errors originating from navigation
  // as the WKWebView will automatically retry these loads.
  WKWebViewErrorSource source = WKWebViewErrorSourceFromError(error);
  return source != NAVIGATION;
}

#pragma mark -
#pragma mark JavaScript message handlers

- (BOOL)handleWindowHistoryWillChangeStateMessage:
    (base::DictionaryValue*)message
                                          context:(NSDictionary*)context {
  _changingHistoryState = YES;
  return
      [super handleWindowHistoryWillChangeStateMessage:message context:context];
}

- (BOOL)handleWindowHistoryDidPushStateMessage:(base::DictionaryValue*)message
                                       context:(NSDictionary*)context {
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;
  return [super handleWindowHistoryDidPushStateMessage:message context:context];
}

- (BOOL)handleWindowHistoryDidReplaceStateMessage:
    (base::DictionaryValue*)message
                                         context:(NSDictionary*)context {
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;
  return [super handleWindowHistoryDidReplaceStateMessage:message
                                                  context:context];
}

#pragma mark -
#pragma mark WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  [super createWebUIForURL:URL];
  _webUIManager.reset(
      [[CRWWebUIManager alloc] initWithWebState:self.webStateImpl]);
}

- (void)clearWebUI {
  [super clearWebUI];
  _webUIManager.reset();
}

#pragma mark -
#pragma mark KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSString* dispatcherSelectorName = self.wkWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName)
    [self performSelector:NSSelectorFromString(dispatcherSelectorName)];
}

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// TODO(eugenebut): use WKWebView progress even if Chrome net stack is enabled.
- (void)webViewEstimatedProgressDidChange {
  if ([self.delegate respondsToSelector:
          @selector(webController:didUpdateProgress:)]) {
    [self.delegate webController:self
               didUpdateProgress:[_wkWebView estimatedProgress]];
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

#endif  // !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

- (void)webViewLoadingStateDidChange {
  if ([_wkWebView isLoading]) {
    [self addActivityIndicatorTask];
  } else {
    [self clearActivityIndicatorTasks];
  }
}

- (void)webViewTitleDidChange {
  if ([self.delegate respondsToSelector:
          @selector(webController:titleDidChange:)]) {
    DCHECK(self.title);
    [self.delegate webController:self titleDidChange:self.title];
  }
}

- (void)webViewURLDidChange {
  // TODO(stuartmorgan): Determine if there are any cases where this still
  // happens, and if so whether anything should be done when it does.
  if (![_wkWebView URL]) {
    DVLOG(1) << "Received nil URL callback";
    return;
  }
  GURL url(net::GURLWithNSURL([_wkWebView URL]));
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
  // If |isLoading| is YES, then it could either be case 1 (if the load phase is
  // web::LOAD_REQUESTED), or it could be case 2 on a page that hasn't finished
  // loading yet. If the domain of the new URL matches the last committed URL,
  // then check window.location.href, and if it matches, trust it. The domain
  // check ensures that if a site somehow corrupts window.location.href it can't
  // do a redirect to a slow-loading target page while it is still loading to
  // spoof the domain. On a document-changing URL change, the
  // window.location.href will match the previous URL at this stage, not the web
  // view's current URL.
  if (![_wkWebView isLoading]) {
    if (_documentURL == url)
      return;
    [self URLDidChangeWithoutDocumentChange:url];
  } else if (!_documentURL.host().empty() &&
             _documentURL.host() == url.host() &&
             self.loadPhase != web::LOAD_REQUESTED) {
    [_wkWebView evaluateJavaScript:@"window.location.href"
                 completionHandler:^(id result, NSError* error) {
                     // If the web view has gone away, or the location
                     // couldn't be retrieved, abort.
                     if (!_wkWebView ||
                         ![result isKindOfClass:[NSString class]]) {
                       return;
                     }
                     GURL jsURL([result UTF8String]);
                     // Make sure that the URL is as expected, and re-check
                     // the host to prevent race conditions.
                     if (jsURL == url && _documentURL.host() == url.host()) {
                       [self URLDidChangeWithoutDocumentChange:url];
                     }
                 }];
  }
}

#pragma mark -
#pragma mark WKNavigationDelegate Methods

- (void)webView:(WKWebView *)webView
    decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
                    decisionHandler:
        (void (^)(WKNavigationActionPolicy))decisionHandler {
  if (self.isBeingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  NSURLRequest* request = navigationAction.request;
  GURL url = net::GURLWithNSURL(request.URL);

  // The page will not be changed until this navigation is commited, so the
  // retrieved referrer will be pending until |didCommitNavigation| callback.
  // Same for the last navigation type.
  [self updatePendingReferrerFromNavigationAction:navigationAction];
  [self updatePendingNavigationTypeForMainFrameFromNavigationAction:
            navigationAction];

  if (navigationAction.sourceFrame.mainFrame)
    self.documentMIMEType = nil;

  web::FrameInfo targetFrame(navigationAction.targetFrame.mainFrame);
  BOOL isLinkClick = [self isLinkNavigation:navigationAction.navigationType];
  BOOL allowLoad = [self shouldAllowLoadWithRequest:request
                                        targetFrame:&targetFrame
                                        isLinkClick:isLinkClick];

  if (allowLoad) {
    allowLoad = self.webStateImpl->ShouldAllowRequest(request);
    _pendingNavigationCancelled = !allowLoad;
  }

  decisionHandler(allowLoad ? WKNavigationActionPolicyAllow
                            : WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView *)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  if ([navigationResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    // Create HTTP headers from the response.
    // TODO(kkhorimoto): Due to the limited interface of NSHTTPURLResponse, some
    // data in the HttpResponseHeaders generated here is inexact.  Once
    // UIWebView is no longer supported, update WebState's implementation so
    // that the Content-Language and the MIME type can be set without using this
    // imperfect conversion.
    scoped_refptr<net::HttpResponseHeaders> HTTPHeaders =
        net::CreateHeadersFromNSHTTPURLResponse(
            static_cast<NSHTTPURLResponse*>(navigationResponse.response));
    self.webStateImpl->OnHttpResponseHeadersReceived(
        HTTPHeaders.get(), net::GURLWithNSURL(navigationResponse.response.URL));
  }
  if (navigationResponse.isForMainFrame)
    self.documentMIMEType = navigationResponse.response.MIMEType;

  BOOL allowNavigation = navigationResponse.canShowMIMEType;
  if (allowNavigation) {
    allowNavigation =
        self.webStateImpl->ShouldAllowResponse(navigationResponse.response);
    _pendingNavigationCancelled = !allowNavigation;
  }

  handler(allowNavigation ? WKNavigationResponsePolicyAllow
                          : WKNavigationResponsePolicyCancel);
}

// TODO(stuartmorgan): Move all the guesswork around these states out of the
// superclass, and wire these up to the remaining methods.
- (void)webView:(WKWebView *)webView
    didStartProvisionalNavigation:(WKNavigation *)navigation {
  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  if (webViewURL.is_empty() && base::ios::IsRunningOnIOS9OrLater()) {
    // May happen on iOS9, however in didCommitNavigation: callback the URL
    // will be "about:blank". TODO(eugenebut): File radar for this issue
    // (crbug.com/523549).
    webViewURL = GURL(url::kAboutBlankURL);
  }

  // Intercept renderer-initiated navigations. If this navigation has not yet
  // been registered, do so. loadPhase check is necessary because
  // lastRegisteredRequestURL may be the same as the webViewURL on a new tab
  // created by window.open (default is about::blank).
  // TODO(jyquinn): Audit [CRWWebController loadWithParams] for other tasks that
  // should be performed here.
  if (self.lastRegisteredRequestURL != webViewURL ||
      self.loadPhase != web::LOAD_REQUESTED) {
    // Reset current WebUI if one exists.
    [self clearWebUI];
    // Restart app specific URL loads to properly capture state.
    // TODO(jyquinn): Extract necessary tasks for app specific URL navigation
    // rather than restarting the load.
    if (web::GetWebClient()->IsAppSpecificURL(webViewURL)) {
      [self abortWebLoad];
      web::WebLoadParams params(webViewURL);
      [self loadWithParams:params];
      return;
    } else {
      [self registerLoadRequest:webViewURL];
    }
  }
  // Ensure the URL is registered and loadPhase is as expected.
  DCHECK(self.lastRegisteredRequestURL == webViewURL);
  DCHECK(self.loadPhase == web::LOAD_REQUESTED);
}

- (void)webView:(WKWebView *)webView
    didReceiveServerRedirectForProvisionalNavigation:
        (WKNavigation *)navigation {
  [self registerLoadRequest:net::GURLWithNSURL(webView.URL)
                   referrer:[self currentReferrer]
                 transition:ui::PAGE_TRANSITION_SERVER_REDIRECT];
}

- (void)webView:(WKWebView *)webView
    didFailProvisionalNavigation:(WKNavigation *)navigation
                       withError:(NSError *)error {
  [self discardPendingReferrerString];

  if (_pendingNavigationCancelled) {
    // Directly cancelled navigations are simply discarded without handling
    // their potential errors.
    _pendingNavigationCancelled = NO;
    return;
  }

  error = WKWebViewErrorWithSource(error, PROVISIONAL_LOAD);

#if defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  // For WKWebViews, the underlying errors for errors reported by the net stack
  // are not copied over when transferring the errors from the IO thread.  For
  // cancelled errors that trigger load abortions, translate the error early to
  // trigger |-discardNonCommittedEntries| from |-handleLoadError:inMainFrame:|.
  if (error.code == NSURLErrorCancelled &&
      [self shouldAbortLoadForCancelledError:error] &&
      !error.userInfo[NSUnderlyingErrorKey]) {
    error = web::NetErrorFromError(error);
  }
#endif  // defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  if (web::IsWKWebViewSSLCertError(error))
    [self handleSSLCertError:error];
  else
#endif
    [self handleLoadError:error inMainFrame:YES];

  [self discardPendingNavigationTypeForMainFrame];
}

- (void)webView:(WKWebView *)webView
    didCommitNavigation:(WKNavigation *)navigation {
  DCHECK_EQ(_wkWebView, webView);
  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  [self injectWindowID];

  // The page has changed; commit the pending referrer.
  [self commitPendingReferrerString];

  // This is the point where the document's URL has actually changed.
  _documentURL = net::GURLWithNSURL([_wkWebView URL]);
  DCHECK(_documentURL == self.lastRegisteredRequestURL);
  [self webPageChanged];

  [self updateCurrentBackForwardListItemHolder];

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  [self updateSSLStatusForCurrentNavigationItem];
#endif

  // Report cases where SSL cert is missing for a secure connection.
  if (_documentURL.SchemeIsCryptographic()) {
    scoped_refptr<net::X509Certificate> cert =
        web::CreateCertFromChain([_wkWebView certificateChain]);
    UMA_HISTOGRAM_BOOLEAN("WebController.WKWebViewHasCertForSecureConnection",
                          cert);
  }
}

- (void)webView:(WKWebView *)webView
    didFinishNavigation:(WKNavigation *)navigation {
  DCHECK(!self.isHalted);
  // Trigger JavaScript driven post-document-load-completion tasks.
  // TODO(jyquinn): Investigate using WKUserScriptInjectionTimeAtDocumentEnd to
  // inject this material at the appropriate time rather than invoking here.
  web::EvaluateJavaScript(webView,
                          @"__gCrWeb.didFinishNavigation()", nil);
  [self didFinishNavigation];
}

- (void)webView:(WKWebView *)webView
    didFailNavigation:(WKNavigation *)navigation
            withError:(NSError *)error {
  [self handleLoadError:WKWebViewErrorWithSource(error, NAVIGATION)
            inMainFrame:YES];
}

- (void)webView:(WKWebView *)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
                    completionHandler:
        (void (^)(NSURLSessionAuthChallengeDisposition disposition,
                  NSURLCredential *credential))completionHandler {
  if (![challenge.protectionSpace.authenticationMethod
          isEqual:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    return;
  }

  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  scoped_refptr<net::X509Certificate> cert = web::CreateCertFromTrust(trust);
  // TODO(eugenebut): pass SecTrustRef instead of cert.
  [_certVerificationController
      decidePolicyForCert:cert
                     host:challenge.protectionSpace.host
        completionHandler:^(web::CertAcceptPolicy policy,
                            net::CertStatus status) {
          completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace,
                            nil);
        }];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self webViewWebProcessDidCrash];
}

#pragma mark WKUIDelegate Methods

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)navigationAction
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  GURL requestURL = net::GURLWithNSURL(navigationAction.request.URL);
  NSString* referer = GetRefererFromNavigationAction(navigationAction);
  GURL referrerURL = referer ? GURL(base::SysNSStringToUTF8(referer)) :
                               [self currentURL];

  if (![self userIsInteracting] &&
      [self shouldBlockPopupWithURL:requestURL sourceURL:referrerURL]) {
    [self didBlockPopupWithURL:requestURL sourceURL:referrerURL];
    // Desktop Chrome does not return a window for the blocked popups;
    // follow the same approach by returning nil;
    return nil;
  }

  id child = [self createChildWebControllerWithReferrerURL:referrerURL];
  // WKWebView requires WKUIDelegate to return a child view created with
  // exactly the same |configuration| object (exception is raised if config is
  // different). |configuration| param and config returned by
  // WKWebViewConfigurationProvider are different objects because WKWebView
  // makes a shallow copy of the config inside init, so every WKWebView
  // owns a separate shallow copy of WKWebViewConfiguration.
  [child ensureWebViewCreatedWithConfiguration:configuration];
  return [child webView];
}

- (void)webView:(WKWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                      initiatedByFrame:(WKFrameInfo*)frame
                     completionHandler:(void(^)())completionHandler {
  SEL alertSelector = @selector(webController:
           runJavaScriptAlertPanelWithMessage:
                                   requestURL:
                            completionHandler:);
  if ([self.UIDelegate respondsToSelector:alertSelector]) {
    [self.UIDelegate webController:self
        runJavaScriptAlertPanelWithMessage:message
                                requestURL:net::GURLWithNSURL(frame.request.URL)
                         completionHandler:completionHandler];
  } else if (completionHandler) {
    completionHandler();
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                        initiatedByFrame:(WKFrameInfo*)frame
                       completionHandler:
        (void (^)(BOOL result))completionHandler {
  SEL confirmationSelector = @selector(webController:
                runJavaScriptConfirmPanelWithMessage:
                                          requestURL:
                                   completionHandler:);
  if ([self.UIDelegate respondsToSelector:confirmationSelector]) {
    [self.UIDelegate webController:self
        runJavaScriptConfirmPanelWithMessage:message
                                  requestURL:
            net::GURLWithNSURL(frame.request.URL)
                           completionHandler:completionHandler];
  } else if (completionHandler) {
    completionHandler(NO);
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                         initiatedByFrame:(WKFrameInfo*)frame
                        completionHandler:
        (void (^)(NSString *result))completionHandler {
  SEL textInputSelector = @selector(webController:
            runJavaScriptTextInputPanelWithPrompt:
                                  placeholderText:
                                       requestURL:
                                completionHandler:);
  if ([self.UIDelegate respondsToSelector:textInputSelector]) {
    [self.UIDelegate webController:self
        runJavaScriptTextInputPanelWithPrompt:prompt
                              placeholderText:defaultText
                                   requestURL:
            net::GURLWithNSURL(frame.request.URL)
                            completionHandler:completionHandler];
  } else if (completionHandler) {
    completionHandler(nil);
  }
}

@end
