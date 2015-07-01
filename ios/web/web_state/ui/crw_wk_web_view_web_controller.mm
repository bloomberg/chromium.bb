// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"

#import <WebKit/WebKit.h>

#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/net/http_response_headers_util.h"
#import "ios/web/crw_network_activity_indicator_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/web_load_params.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/ui_web_view_util.h"
#include "ios/web/web_state/blocked_popup_info.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_wk_web_view_crash_detector.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/webui/crw_web_ui_manager.h"
#import "net/base/mac/url_conversions.h"

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
#include "ios/web/public/cert_store.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#include "net/ssl/ssl_info.h"
#endif

namespace {
// Extracts Referer value from WKNavigationAction request header.
NSString* GetRefererFromNavigationAction(WKNavigationAction* action) {
  return [action.request valueForHTTPHeaderField:@"Referer"];
}

NSString* const kScriptMessageName = @"crwebinvoke";
NSString* const kScriptImmediateName = @"crwebinvokeimmediate";

}  // namespace

@interface CRWWKWebViewWebController () <WKNavigationDelegate,
                                         WKScriptMessageHandler,
                                         WKUIDelegate> {
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

  // Whether the web page is currently performing window.history.pushState or
  // window.history.replaceState
  // Set to YES on window.history.willChangeState message. To NO on
  // window.history.didPushState or window.history.didReplaceState.
  BOOL _changingHistoryState;

  // CRWWebUIManager object for loading WebUI pages.
  base::scoped_nsobject<CRWWebUIManager> _webUIManager;
}

// Response's MIME type of the last known navigation.
@property(nonatomic, copy) NSString* documentMIMEType;

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(nonatomic, readonly) NSDictionary* wkWebViewObservers;

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
// Called when a load ends in an SSL error.
- (void)handleSSLError:(NSError*)error;
#endif

// Adds an activity indicator tasks for this web controller.
- (void)addActivityIndicatorTask;

// Clears all activity indicator tasks for this web controller.
- (void)clearActivityIndicatorTasks;

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

// Returns new autoreleased instance of WKUserContentController which has
// early page script.
- (WKUserContentController*)createUserContentController;

// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange;

// Called when WKWebView hasOnlySecureContent property has changed.
- (void)webViewContentSecurityDidChange;
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
  return [super initWithWebState:webState.Pass()];
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

- (void)registerUserAgent {
  // TODO(stuartmorgan): Rename this method, since it works for both.
  web::BuildAndRegisterUserAgentForUIWebView(
      self.webStateImpl->GetRequestGroupID(),
      [self useDesktopUserAgent]);
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

- (BOOL)shouldAbortLoadForCancelledURL:(const GURL&)cancelledURL {
  // Do not abort the load if it is for an app specific URL, as such errors
  // are produced during the app specific URL load process.
  return !web::GetWebClient()->IsAppSpecificURL(cancelledURL);
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
    @"hasOnlySecureContent" : @"webViewContentSecurityDidChange",
#endif
    @"loading" : @"webViewLoadingStateDidChange",
    @"title" : @"webViewTitleDidChange",
    @"URL" : @"webViewURLDidChange",
  };
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
    // Use a separate userContentController for each web view.
    // WKUserContentController does not allow adding multiple script message
    // handlers for the same name, hence userContentController can't be shared
    // between all web views.
    config.userContentController = [self createUserContentController];
    [self setWebView:[self createWebViewWithConfiguration:config]];
    // Notify super class about created web view. -webViewDidChange is not
    // called from -setWebView:scriptMessageRouter: as the latter used in unit
    // tests with fake web view, which cannot be added to view hierarchy.
    [self webViewDidChange];
  }
}

- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config {
  return [web::CreateWKWebView(
              CGRectZero,
              config,
              self.webStateImpl->GetBrowserState(),
              self.webStateImpl->GetRequestGroupID(),
              [self useDesktopUserAgent]) autorelease];
}

- (void)setWebView:(WKWebView*)webView {
  DCHECK_NE(_wkWebView.get(), webView);

  // Unwind the old web view.
  WKUserContentController* oldContentController =
      [[_wkWebView configuration] userContentController];
  [oldContentController removeScriptMessageHandlerForName:kScriptMessageName];
  [oldContentController removeScriptMessageHandlerForName:kScriptImmediateName];
  [_wkWebView setNavigationDelegate:nil];
  [_wkWebView setUIDelegate:nil];
  for (NSString* keyPath in self.wkWebViewObservers) {
    [_wkWebView removeObserver:self forKeyPath:keyPath];
  }
  [self clearActivityIndicatorTasks];

  _wkWebView.reset([webView retain]);

  // Set up the new web view.
  WKUserContentController* newContentController =
      [[_wkWebView configuration] userContentController];
  [newContentController addScriptMessageHandler:self name:kScriptMessageName];
  [newContentController addScriptMessageHandler:self name:kScriptImmediateName];
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
  if (action.targetFrame.isMainFrame)
    [self setPendingReferrerString:GetRefererFromNavigationAction(action)];
}

- (void)commitPendingReferrerString {
  _currentReferrerString.reset(_pendingReferrerString.release());
}

- (void)discardPendingReferrerString {
  _pendingReferrerString.reset();
}

- (CRWWKWebViewCrashDetector*)newCrashDetectorWithWebView:(WKWebView*)webView {
  if (!webView)
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
- (void)handleSSLError:(NSError*)error {
  DCHECK(web::IsWKWebViewSSLError(error));

  net::SSLInfo sslInfo;
  web::GetSSLInfoFromWKWebViewSSLError(error, &sslInfo);

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

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
- (void)updateSSLStatusForCurrentNavigationItem {
  DCHECK(self.webStateImpl);
  web::NavigationItem* item =
      self.webStateImpl->GetNavigationManagerImpl().GetLastCommittedItem();
  if (!item)
    return;

  // WKWebView will not load unauthenticated content.
  item->GetSSL().security_style = item->GetURL().SchemeIsCryptographic()
                                      ? web::SECURITY_STYLE_AUTHENTICATED
                                      : web::SECURITY_STYLE_UNAUTHENTICATED;
  int contentStatus = [_wkWebView hasOnlySecureContent] ?
      web::SSLStatus::NORMAL_CONTENT :
      web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  item->GetSSL().content_status = contentStatus;
  [self didUpdateSSLStatusForCurrentNavigationItem];
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

- (WKUserContentController*)createUserContentController {
  WKUserContentController* result =
      [[[WKUserContentController alloc] init] autorelease];
  base::scoped_nsobject<WKUserScript> script([[WKUserScript alloc]
        initWithSource:web::GetEarlyPageScript(web::WK_WEB_VIEW_TYPE)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES]);
  [result addUserScript:script];
  return result;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
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

- (void)webViewContentSecurityDidChange {
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
  // If |isLoading| is YES, then it could either be case 1, or it could be
  // case 2 on a page that hasn't finished loading yet. If the domain of the
  // new URL matches the last committed URL, then check window.location.href,
  // and if it matches, trust it. The domain check ensures that if a site
  // somehow corrupts window.location.href it can't do a redirect to a
  // slow-loading target page while it is still loading to spoof the domain.
  // On a document-changing URL change, the window.location.href will match the
  // previous URL at this stage, not the web view's current URL.
  if (![_wkWebView isLoading]) {
    if (_documentURL == url)
      return;
    [self URLDidChangeWithoutDocumentChange:url];
  } else if (!_documentURL.host().empty() &&
             _documentURL.host() == url.host()) {
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
  [self updatePendingReferrerFromNavigationAction:navigationAction];

  if (navigationAction.sourceFrame.isMainFrame)
    self.documentMIMEType = nil;

  web::FrameInfo targetFrame(navigationAction.targetFrame.isMainFrame);
  BOOL isLinkClick = [self isLinkNavigation:navigationAction.navigationType];
  BOOL allowLoad = [self shouldAllowLoadWithRequest:request
                                        targetFrame:&targetFrame
                                        isLinkClick:isLinkClick];
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
  handler(navigationResponse.canShowMIMEType ? WKNavigationResponsePolicyAllow :
              WKNavigationResponsePolicyCancel);
}

// TODO(stuartmorgan): Move all the guesswork around these states out of the
// superclass, and wire these up to the remaining methods.
- (void)webView:(WKWebView *)webView
    didStartProvisionalNavigation:(WKNavigation *)navigation {
  GURL webViewURL = net::GURLWithNSURL(webView.URL);
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

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  if (web::IsWKWebViewSSLError(error))
    [self handleSSLError:error];
  else
#endif
    [self handleLoadError:error inMainFrame:YES];
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

#if !defined(ENABLE_CHROME_NET_STACK_FOR_WKWEBVIEW)
  [self updateSSLStatusForCurrentNavigationItem];
#endif
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
  [self handleLoadError:error inMainFrame:YES];
}

- (void)webView:(WKWebView *)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
                    completionHandler:
        (void (^)(NSURLSessionAuthChallengeDisposition disposition,
                  NSURLCredential *credential))completionHandler {
  NOTIMPLEMENTED();
  completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
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
