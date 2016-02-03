// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"

#import <WebKit/WebKit.h>
#include <stddef.h>

#include <utility>

#include "base/containers/mru_cache.h"
#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#import "base/mac/objc_property_releaser.h"
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
#include "ios/web/net/cert_host_pair.h"
#import "ios/web/net/crw_cert_verification_controller.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/cert_store.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_kit_constants.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/ui_web_view_util.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/crw_pass_kit_downloader.h"
#import "ios/web/web_state/error_translation_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_post_request_loader.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "ios/web/webui/crw_web_ui_manager.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "url/url_constants.h"

namespace {

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

#pragma mark -

@interface CRWWKWebViewWebController ()<WKNavigationDelegate, WKUIDelegate> {
  // The WKWebView managed by this instance.
  base::scoped_nsobject<WKWebView> _wkWebView;

  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/549616): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative. For now, this allows
  // sharing the flow that's currently in the superclass.
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

  // Pending information for an in-progress page navigation. The lifetime of
  // this object starts at |decidePolicyForNavigationAction| where the info is
  // extracted from the request, and ends at either |didCommitNavigation| or
  // |didFailProvisionalNavigation|.
  base::scoped_nsobject<CRWWebControllerPendingNavigationInfo>
      _pendingNavigationInfo;

  // Referrer for the current page.
  base::scoped_nsobject<NSString> _currentReferrerString;

  // Handles downloading PassKit data for WKWebView. Lazy initialized.
  base::scoped_nsobject<CRWPassKitDownloader> _passKitDownloader;

  // Object for loading POST requests with body.
  base::scoped_nsobject<CRWJSPOSTRequestLoader> _POSTRequestLoader;

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

  // CertVerification errors which happened inside
  // |webView:didReceiveAuthenticationChallenge:completionHandler:|.
  // Key is leaf-cert/host pair. This storage is used to carry calculated
  // cert status from |didReceiveAuthenticationChallenge:| to
  // |didFailProvisionalNavigation:| delegate method.
  scoped_ptr<CertVerificationErrorsCacheType> _certVerificationErrors;

  // YES if the user has interacted with the content area since the last URL
  // change.
  BOOL _interactionRegisteredSinceLastURLChange;

  // YES if the web process backing _wkWebView is believed to currently be dead.
  BOOL _webProcessIsDead;

  // The WKNavigation for the most recent load request.
  base::scoped_nsobject<WKNavigation> _latestWKNavigation;

  // The WKNavigation captured when |stopLoading| was called. Used for reporting
  // WebController.EmptyNavigationManagerCausedByStopLoading UMA metric which
  // helps with diagnosing a navigation related crash (crbug.com/565457).
  base::WeakNSObject<WKNavigation> _stoppedWKNavigation;
}

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(nonatomic, readonly) NSDictionary* wkWebViewObservers;

// Activity indicator group ID for this web controller.
@property(nonatomic, readonly) NSString* activityIndicatorGroupID;

// Identifier used for storing and retrieving certificates.
@property(nonatomic, readonly) int certGroupID;

// Downloader for PassKit files. Lazy initialized.
@property(nonatomic, readonly) CRWPassKitDownloader* passKitDownloader;

// Loads POST request with body in |_wkWebView| by constructing an HTML page
// that executes the request through JavaScript and replaces document with the
// result.
// Note that this approach includes multiple body encodings and decodings, plus
// the data is passed to |_wkWebView| on main thread.
// This is necessary because WKWebView ignores POST request body.
// Workaround for https://bugs.webkit.org/show_bug.cgi?id=145410
- (void)loadPOSTRequest:(NSMutableURLRequest*)request;

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

// Sets _documentURL to newURL, and updates any relevant state information.
- (void)setDocumentURL:(const GURL&)newURL;

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationAction|, but must be in a pending state until
// |didCommitNavigation| where it becames current.
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

// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder;

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;

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

// Called when a load ends in an SSL error and certificate chain.
- (void)handleSSLCertError:(NSError*)error;

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

// Updates SSL status for the current navigation item based on the information
// provided by web view.
- (void)updateSSLStatusForCurrentNavigationItem;

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

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to
// reply with NSURLSessionAuthChallengeDisposition and credentials.
- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler;

// Registers load request with empty referrer and link or client redirect
// transition based on user interaction state.
- (void)registerLoadRequest:(const GURL&)url;

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

// Called when web controller receives a new message from the web page.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message;

// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;

// Used to decide whether a load that generates errors with the
// NSURLErrorCancelled code should be cancelled.
- (BOOL)shouldAbortLoadForCancelledError:(NSError*)error;

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

@end

@implementation CRWWKWebViewWebController

#pragma mark CRWWebController public methods

- (instancetype)initWithWebState:(scoped_ptr<web::WebStateImpl>)webState {
  DCHECK(webState);
  web::BrowserState* browserState = webState->GetBrowserState();
  self = [super initWithWebState:std::move(webState)];
  if (self) {
    _certVerificationController.reset([[CRWCertVerificationController alloc]
        initWithBrowserState:browserState]);
    _certVerificationErrors.reset(
        new CertVerificationErrorsCacheType(kMaxCertErrorsCount));
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

- (void)terminateNetworkActivity {
  web::CertStore::GetInstance()->RemoveCertsForGroup(self.certGroupID);
  [super terminateNetworkActivity];
}

- (void)setPageDialogOpenPolicy:(web::PageDialogOpenPolicy)policy {
  // TODO(eugenebut): implement dialogs/windows suppression using
  // WKNavigationDelegate methods where possible.
  [super setPageDialogOpenPolicy:policy];
}

- (void)close {
  [_certVerificationController shutDown];
  [super close];
}

- (void)stopLoading {
  _stoppedWKNavigation.reset(_latestWKNavigation);
  [super stopLoading];
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

// Overridden to track interactions since URL change.
- (void)setUserInteractionRegistered:(BOOL)flag {
  [super setUserInteractionRegistered:flag];
  if (flag)
    _interactionRegisteredSinceLastURLChange = YES;
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

- (BOOL)isCurrentNavigationItemPOST {
  // |_pendingNavigationInfo| will be nil if the decidePolicy* delegate methods
  // were not called.
  WKNavigationType type =
      _pendingNavigationInfo
          ? [_pendingNavigationInfo navigationType]
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

  std::string MIMEType = self.webState->GetContentsMimeType();
  return [self documentTypeFromMIMEType:base::SysUTF8ToNSString(MIMEType)];
}

- (void)loadRequest:(NSMutableURLRequest*)request {
  _latestWKNavigation.reset([[_wkWebView loadRequest:request] retain]);
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
  DCHECK([self currentSessionEntry]);

  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];
  BOOL isFormResubmission =
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem =
      [self currentSessionEntry].navigationItemImpl;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  // If the request has POST data and is not a form resubmission, configure and
  // run the POST request.
  if (POSTData.length && !isFormResubmission) {
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
    if ([self currentNavigationURL] == net::GURLWithNSURL([_wkWebView URL])) {
      [_wkWebView reload];
    } else {
      [_wkWebView goToBackForwardListItem:holder->back_forward_list_item()];
    }
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.

  if (!isFormResubmission ||
      currentItem->ShouldSkipResubmitDataConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  DCHECK(isFormResubmission);
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
  [_pendingNavigationInfo setCancelled:YES];
  _certVerificationErrors->Clear();
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

- (void)loadCompletedForURL:(const GURL&)loadedURL {
  // Nothing to do.
}

// Override |handleLoadError| to check for PassKit case.
- (void)handleLoadError:(NSError*)error inMainFrame:(BOOL)inMainFrame {
  NSString* MIMEType = [_pendingNavigationInfo MIMEType];
  if ([self.passKitDownloader isMIMETypePassKitType:MIMEType])
    return;
  [super handleLoadError:error inMainFrame:inMainFrame];
}

// Override |loadCancelled| to |cancelPendingDownload| for the
// CRWPassKitDownloader.
- (void)loadCancelled {
  [_passKitDownloader cancelPendingDownload];
  [super loadCancelled];
}

- (BOOL)isViewAlive {
  return !_webProcessIsDead && [super isViewAlive];
}

#pragma mark Private methods

- (NSDictionary*)wkWebViewObservers {
  return @{
    @"certificateChain" : @"webViewSecurityFeaturesDidChange",
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"hasOnlySecureContent" : @"webViewSecurityFeaturesDidChange",
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

- (int)certGroupID {
  DCHECK(self.webStateImpl);
  // Request tracker IDs are used as certificate groups.
  return self.webStateImpl->GetRequestTracker()->identifier();
}

- (void)loadPOSTRequest:(NSMutableURLRequest*)request {
  if (!_POSTRequestLoader) {
    _POSTRequestLoader.reset([[CRWJSPOSTRequestLoader alloc] init]);
  }

  CRWWKScriptMessageRouter* messageRouter =
      [self webViewConfigurationProvider].GetScriptMessageRouter();

  [_POSTRequestLoader loadPOSTRequest:request
                            inWebView:_wkWebView
                        messageRouter:messageRouter
                    completionHandler:^(NSError* loadError) {
                      if (loadError)
                        [self handleLoadError:loadError inMainFrame:YES];
                      else
                        self.webStateImpl->SetContentsMimeType("text/html");
                    }];
}

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
  [self setDocumentURL:[self defaultURL]];
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

- (void)setDocumentURL:(const GURL&)newURL {
  if (newURL != _documentURL) {
    _documentURL = newURL;
    _interactionRegisteredSinceLastURLChange = NO;
  }
}

- (void)updatePendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame) {
    _pendingNavigationInfo.reset(
        [[CRWWebControllerPendingNavigationInfo alloc] init]);
    [_pendingNavigationInfo setReferrer:GetRefererFromNavigationAction(action)];
    [_pendingNavigationInfo setNavigationType:action.navigationType];
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
  holder->set_back_forward_list_item([_wkWebView backForwardList].currentItem);
  holder->set_navigation_type(navigationType);

  // Only update the MIME type in the holder if there was MIME type information
  // as part of this pending load. It will be nil when doing a fast
  // back/forward navigation, for instance, because the callback that would
  // populate it is not called in that flow.
  if ([_pendingNavigationInfo MIMEType])
    holder->set_mime_type([_pendingNavigationInfo MIMEType]);
}

- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = [_wkWebView backForwardList];
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

- (void)webViewWebProcessDidCrash {
  _webProcessIsDead = YES;

  SEL cancelDialogsSelector = @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];

  SEL rendererCrashSelector = @selector(webControllerWebProcessDidCrash:);
  if ([self.delegate respondsToSelector:rendererCrashSelector])
    [self.delegate webControllerWebProcessDidCrash:self];
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

- (void)handleSSLCertError:(NSError*)error {
  DCHECK(web::IsWKWebViewSSLCertError(error));

  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);

  web::SSLStatus status;
  status.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  status.cert_status = info.cert_status;
  status.cert_id = web::CertStore::GetInstance()->StoreCert(
      info.cert.get(), self.certGroupID);

  // Retrieve verification results from _certVerificationErrors cache to avoid
  // unnecessary recalculations. Verification results are cached for the leaf
  // cert, because the cert chain in |didReceiveAuthenticationChallenge:| is
  // the OS constructed chain, while |chain| is the chain from the server.
  NSArray* chain = error.userInfo[web::kNSErrorPeerCertificateChainKey];
  NSString* host = [error.userInfo[web::kNSErrorFailingURLKey] host];
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
        status.cert_status = error->second.status;
        info.cert_status = error->second.status;
      }
      UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                            cacheHit);
    }
  }

  // Present SSL interstitial and inform everyone that the load is cancelled.
  [self.delegate presentSSLError:info
                    forSSLStatus:status
                     recoverable:recoverable
                        callback:^(BOOL proceed) {
                          if (proceed) {
                            // The interstitial will be removed during reload.
                            [_certVerificationController
                                allowCert:leafCert
                                  forHost:host
                                   status:status.cert_status];
                            [self loadCurrentURL];
                          }
                        }];
  [self loadCancelled];
}

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
  for (int i = navigationManager->GetItemCount() - 1; 0 <= i; i--) {
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
      if (navigationManager->GetCurrentItemIndex() == i &&
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

- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler {
  NSURLProtectionSpace* space = challenge.protectionSpace;
  DCHECK(
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPDigest]);

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
                      [CRWWKWebViewWebController
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

- (void)registerLoadRequest:(const GURL&)url {
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
  [self registerLoadRequest:url referrer:emptyReferrer transition:transition];
}

- (BOOL)isKVOChangePotentialSameDocumentNavigationToURL:(const GURL&)newURL {
  DCHECK([_wkWebView isLoading]);
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
  DCHECK(newURL == net::GURLWithNSURL([_wkWebView URL]));
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

- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  // Broken out into separate method to catch errors.
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

- (CRWPassKitDownloader*)passKitDownloader {
  if (_passKitDownloader) {
    return _passKitDownloader.get();
  }
  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  web::PassKitCompletionHandler passKitCompletion = ^(NSData* data) {
    base::scoped_nsobject<CRWWKWebViewWebController> strongSelf(
        [weakSelf retain]);
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
#pragma mark CRWWebViewScrollViewProxyObserver

// Under WKWebView, JavaScript can execute asynchronously. User can start
// scrolling and calls to window.scrollTo executed during scrolling will be
// treated as "during user interaction" and can cause app to go fullscreen.
// This is a workaround to use this webViewScrollViewIsDragging flag to ignore
// window.scrollTo while user is scrolling. See crbug.com/554257
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self evaluateJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(true)"
       stringResultHandler:nil];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  [self evaluateJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(false)"
       stringResultHandler:nil];
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

- (void)webViewLoadingStateDidChange {
  if ([_wkWebView isLoading]) {
    [self addActivityIndicatorTask];
  } else {
    [self clearActivityIndicatorTasks];
    if ([self currentNavItem] &&
        [self currentBackForwardListItemHolder]->navigation_type() ==
            WKNavigationTypeBackForward) {
      // A fast back/forward may not call |webView:didFinishNavigation:|, so
      // finishing the navigation should be signalled explicitly.
      [self didFinishNavigation];
    }
  }
}

- (void)webViewTitleDidChange {
  // WKWebView's title becomes empty when the web process dies; ignore that
  // update.
  if (_webProcessIsDead) {
    DCHECK_EQ(self.title.length, 0U);
    return;
  }

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
  if (![_wkWebView isLoading]) {
    if (_documentURL == url)
      return;
    [self URLDidChangeWithoutDocumentChange:url];
  } else if ([self isKVOChangePotentialSameDocumentNavigationToURL:url]) {
    [_wkWebView evaluateJavaScript:@"window.location.href"
                 completionHandler:^(id result, NSError* error) {
                     // If the web view has gone away, or the location
                     // couldn't be retrieved, abort.
                     if (!_wkWebView ||
                         ![result isKindOfClass:[NSString class]]) {
                       return;
                     }
                     GURL jsURL([result UTF8String]);
                     // Check that window.location matches the new URL. If
                     // it does not, this is a document-changing URL change as
                     // the window location would not have changed to the new
                     // URL when the script was called.
                     BOOL windowLocationMatchesNewURL = jsURL == url;
                     // Re-check origin in case navigaton has occured since
                     // start of JavaScript evaluation.
                     BOOL newURLOriginMatchesDocumentURLOrigin =
                         _documentURL.GetOrigin() == url.GetOrigin();
                     // Check that the web view URL still matches the new URL.
                     // TODO(crbug.com/563568): webViewURLMatchesNewURL check
                     // may drop same document URL changes if pending URL
                     // change occurs immediately after. Revisit heuristics to
                     // prevent this.
                     BOOL webViewURLMatchesNewURL =
                         net::GURLWithNSURL([_wkWebView URL]) == url;
                     // Check that the new URL is different from the current
                     // document URL. If not, URL change should not be reported.
                     BOOL URLDidChangeFromDocumentURL = url != _documentURL;
                     if (windowLocationMatchesNewURL &&
                         newURLOriginMatchesDocumentURLOrigin &&
                         webViewURLMatchesNewURL &&
                         URLDidChangeFromDocumentURL) {
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
  _webProcessIsDead = NO;
  if (self.isBeingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  NSURLRequest* request = navigationAction.request;
  GURL url = net::GURLWithNSURL(request.URL);

  // The page will not be changed until this navigation is commited, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationAction:navigationAction];

  web::FrameInfo targetFrame(navigationAction.targetFrame.mainFrame);
  BOOL isLinkClick = [self isLinkNavigation:navigationAction.navigationType];
  BOOL allowLoad = [self shouldAllowLoadWithRequest:request
                                        targetFrame:&targetFrame
                                        isLinkClick:isLinkClick];

  if (allowLoad) {
    allowLoad = self.webStateImpl->ShouldAllowRequest(request);
    if (!allowLoad && navigationAction.targetFrame.mainFrame) {
      [_pendingNavigationInfo setCancelled:YES];
    }
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
  if (self.lastRegisteredRequestURL != webViewURL ||
      self.loadPhase != web::LOAD_REQUESTED) {
    // Reset current WebUI if one exists.
    [self clearWebUI];
    // Restart app specific URL loads to properly capture state.
    // TODO(crbug.com/546347): Extract necessary tasks for app specific URL
    // navigation rather than restarting the load.
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
  _latestWKNavigation.reset([navigation retain]);
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

  // Directly cancelled navigations are simply discarded without handling
  // their potential errors.
  if (![_pendingNavigationInfo cancelled]) {
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

- (void)webView:(WKWebView *)webView
    didCommitNavigation:(WKNavigation *)navigation {
  DCHECK_EQ(_wkWebView, webView);
  _certVerificationErrors->Clear();
  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  [self injectWindowID];

  // This is the point where the document's URL has actually changed, and
  // pending navigation information should be applied to state information.
  [self setDocumentURL:net::GURLWithNSURL([_wkWebView URL])];
  DCHECK(_documentURL == self.lastRegisteredRequestURL);
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
  // TODO(crbug.com/546350): Investigate using
  // WKUserScriptInjectionTimeAtDocumentEnd to inject this material at the
  // appropriate time rather than invoking here.
  web::EvaluateJavaScript(webView,
                          @"__gCrWeb.didFinishNavigation()", nil);
  [self didFinishNavigation];
}

- (void)webView:(WKWebView *)webView
    didFailNavigation:(WKNavigation *)navigation
            withError:(NSError *)error {
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
  base::WeakNSObject<CRWWKWebViewWebController> weakSelf(self);
  [_certVerificationController
      decideLoadPolicyForTrust:scopedTrust
                          host:challenge.protectionSpace.host
             completionHandler:^(web::CertAcceptPolicy policy,
                                 net::CertStatus status) {
               base::scoped_nsobject<CRWWKWebViewWebController> strongSelf(
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

#pragma mark WKUIDelegate Methods

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)navigationAction
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  GURL requestURL = net::GURLWithNSURL(navigationAction.request.URL);

  if (![self userIsInteracting]) {
    NSString* referer = GetRefererFromNavigationAction(navigationAction);
    GURL referrerURL =
        referer ? GURL(base::SysNSStringToUTF8(referer)) : [self currentURL];
    if ([self shouldBlockPopupWithURL:requestURL sourceURL:referrerURL]) {
      [self didBlockPopupWithURL:requestURL sourceURL:referrerURL];
      // Desktop Chrome does not return a window for the blocked popups;
      // follow the same approach by returning nil;
      return nil;
    }
  }

  id child = [self createChildWebController];
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
  } else if (completionHandler) {
    completionHandler(nil);
  }
}

@end
