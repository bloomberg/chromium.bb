// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"

#include <stddef.h>

#include <utility>

#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_kit_constants.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/web_state/error_translation_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_post_request_loader.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "url/url_constants.h"

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

@interface CRWWKWebViewWebController () {
  // The WKWebView managed by this instance.
  base::scoped_nsobject<WKWebView> _wkWebView;

  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/549616): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative. For now, this allows
  // sharing the flow that's currently in the superclass.
  GURL _documentURL;

  // Object for loading POST requests with body.
  base::scoped_nsobject<CRWJSPOSTRequestLoader> _POSTRequestLoader;

  // YES if the user has interacted with the content area since the last URL
  // change.
  BOOL _interactionRegisteredSinceLastURLChange;
}

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(nonatomic, readonly) NSDictionary* wkWebViewObservers;

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

// Returns a new autoreleased web view created with given configuration.
- (WKWebView*)createWebViewWithConfiguration:(WKWebViewConfiguration*)config;

// Sets the value of the webView property, and performs its basic setup.
- (void)setWebView:(WKWebView*)webView;

// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;

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

#pragma mark -
#pragma mark Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  [super injectWebViewContentView:webViewContentView];
  [self setWebView:static_cast<WKWebView*>(webViewContentView.webView)];
}

#pragma mark - Protected property implementations

- (WKWebView*)webView {
  return _wkWebView.get();
}

- (UIScrollView*)webScrollView {
  return [_wkWebView scrollView];
}

- (NSString*)title {
  return [_wkWebView title];
}

- (GURL)documentURL {
  return _documentURL;
}

- (BOOL)interactionRegisteredSinceLastURLChange {
  return _interactionRegisteredSinceLastURLChange;
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

- (void)willLoadCurrentURLInWebView {
  // TODO(stuartmorgan): Get a WKWebView version of the request ID verification
  // code working for debug builds.
}

- (void)loadRequestForCurrentNavigationItem {
  DCHECK(self.webView && !self.nativeController);
  DCHECK([self currentSessionEntry]);
  // If a load is kicked off on a WKWebView with a frame whose size is {0, 0} or
  // that has a negative dimension for a size, rendering issues occur that
  // manifest in erroneous scrolling and tap handling (crbug.com/574996,
  // crbug.com/577793).
  DCHECK_GT(CGRectGetWidth(self.webView.frame), 0.0);
  DCHECK_GT(CGRectGetHeight(self.webView.frame), 0.0);

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

- (void)abortWebLoad {
  [_wkWebView stopLoading];
  [self.pendingNavigationInfo setCancelled:YES];
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

// Override |handleLoadError| to check for PassKit case.
- (void)handleLoadError:(NSError*)error inMainFrame:(BOOL)inMainFrame {
  NSString* MIMEType = [self.pendingNavigationInfo MIMEType];
  if ([self.passKitDownloader isMIMETypePassKitType:MIMEType])
    return;
  [super handleLoadError:error inMainFrame:inMainFrame];
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
  [self clearInjectedScriptManagers];
  [self setDocumentURL:[self defaultURL]];
}

- (void)setDocumentURL:(const GURL&)newURL {
  if (newURL != _documentURL) {
    _documentURL = newURL;
    _interactionRegisteredSinceLastURLChange = NO;
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

- (void)webViewWebProcessDidCrash {
  [self setWebProcessIsDead:YES];

  SEL cancelDialogsSelector = @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];

  SEL rendererCrashSelector = @selector(webControllerWebProcessDidCrash:);
  if ([self.delegate respondsToSelector:rendererCrashSelector])
    [self.delegate webControllerWebProcessDidCrash:self];
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
  if (!self.changingHistoryState) {
    // If this wasn't a previously-expected load (e.g., certain back/forward
    // navigations), register the load request.
    if (![self isLoadRequestPendingForURL:newURL])
      [self registerLoadRequest:newURL];
  }

  [self setDocumentURL:newURL];

  if (!self.changingHistoryState) {
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
  if (self.webProcessIsDead) {
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

@end
