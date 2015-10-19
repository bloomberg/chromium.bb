// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_ui_web_view_web_controller.h"

#import "base/ios/ns_error_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/mac/bind_objc_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "base/values.h"
#import "ios/net/nsurlrequest_util.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/net/clients/crw_redirect_network_client_factory.h"
#import "ios/web/net/crw_url_verifying_protocol_handler.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/ui_web_view_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_invoke_parameter_queue.h"
#import "ios/web/web_state/ui/crw_context_menu_provider.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "url/url_constants.h"

namespace web {

// The following continuous check timer frequency constants are externally
// available for the purpose of performance tests.
// Frequency for the continuous checks when a reset in the page object is
// anticipated shortly. In milliseconds.
const int64 kContinuousCheckIntervalMSHigh = 100;

// The maximum duration that the CRWWebController can run in high-frequency
// check mode before being changed back to the low frequency.
const int64 kContinuousCheckHighFrequencyMSMaxDuration = 5000;

// Frequency for the continuous checks when a reset in the page object is not
// anticipated; checks are only made as a precaution.
// The URL could be out of date for this many milliseconds, so this should not
// be increased without careful consideration.
const int64 kContinuousCheckIntervalMSLow = 3000;

}  // namespace web

@interface CRWUIWebViewWebController () <CRWRedirectClientDelegate,
                                         UIWebViewDelegate> {
  // The UIWebView managed by this instance.
  base::scoped_nsobject<UIWebView> _uiWebView;

  // Whether caching of the current URL is enabled or not.
  BOOL _urlCachingEnabled;

  // Temporarily cached current URL. Only valid/set while urlCachingEnabled
  // is YES.
  // TODO(stuartmorgan): Change this to a struct so code using it is more
  // readable.
  std::pair<GURL, web::URLVerificationTrustLevel> _cachedURL;

  // The last time a URL with absolute trust level was computed.
  // When an untrusted URL is retrieved from the
  // |CRWURLVerifyingProtocolHandler|, if the last trusted URL is within
  // |kContinuousCheckIntervalMSLow|, the trustLevel is upgraded to Mixed.
  // The reason is that it is sometimes temporarily impossible to do a
  // AsyncXMLHttpRequest on a web page. When this happen, it is not possible
  // to check for the validity of the current URL. Because the checker is
  // only checking every |kContinuousCheckIntervalMSLow| anyway, waiting this
  // amount of time before triggering an interstitial does not weaken the
  // security of the browser.
  base::TimeTicks _lastCorrectURLTime;

  // Each new UIWebView starts in a state where:
  // - window.location.href is equal to about:blank
  // - Ajax requests seem to be impossible
  // Because Ajax requests are used to determine is a URL is verified, this
  // means it is impossible to do this check until the UIWebView is in a more
  // sane state. This variable tracks whether verifying the URL is currently
  // impossible. It starts at YES when a new UIWebView is created,
  // and will change to NO, as soon as either window.location.href is not
  // about:blank anymore, or an URL verification request succeeds. This means
  // that a malicious site that is able to change the value of
  // window.location.href and that is loaded as the first request will be able
  // to change its URL to about:blank. As this is not an interesting URL, it is
  // considered acceptable.
  BOOL _spoofableRequest;

  // Timer used to make continuous checks on the UIWebView.  Timer is
  // running only while |webView| is non-nil.
  scoped_ptr<base::Timer> _continuousCheckTimer;
  // Timer to lower the check frequency automatically.
  scoped_ptr<base::Timer> _lowerFrequencyTimer;

  // Counts of calls to |-webViewDidStartLoad:| and |-webViewDidFinishLoad|.
  // When |_loadCount| is equal to |_unloadCount|, the page is no longer
  // loading. Used as a fallback to determine when the page is done loading in
  // case document.readyState isn't sufficient.
  // When |_loadCount| is 1, the main page is loading (as opposed to a
  // sub-resource).
  int _loadCount;
  int _unloadCount;

  // Backs the property of the same name.
  BOOL _inJavaScriptContext;

  // Backs the property of the same name.
  base::scoped_nsobject<CRWJSInvokeParameterQueue> _jsInvokeParameterQueue;

  // Blocks message queue processing (for testing).
  BOOL _jsMessageQueueThrottled;

  // YES if a video is playing in fullscreen.
  BOOL _inFullscreenVideo;

  // Backs the property of the same name.
  id<CRWRecurringTaskDelegate>_recurringTaskDelegate;

  // Redirect client factory.
  base::scoped_nsobject<CRWRedirectNetworkClientFactory>
      redirect_client_factory_;
}

// Whether or not URL caching is enabled. Between enabling and disabling
// caching, calls to webURLWithTrustLevel: after the first may return the same
// answer without re-checking the URL.
@property(nonatomic, setter=setURLCachingEnabled:) BOOL urlCachingEnabled;

// Returns whether the current page is the web views initial (default) page.
- (BOOL)isDefaultPage;

// Returns whether the given navigation is triggered by a user link click.
- (BOOL)isLinkNavigation:(UIWebViewNavigationType)navigationType;

// Starts (at the given interval) the timer that drives runRecurringTasks to see
// whether or not the page has changed. This is used to work around the fact
// that UIWebView callbacks are not sufficiently reliable to catch every page
// change.
- (void)setContinuousCheckTimerInterval:(const base::TimeDelta&)interval;

// Evaluates the supplied JavaScript and returns the result. Will return nil
// if it is unable to evaluate the JavaScript.
- (NSString*)stringByEvaluatingJavaScriptFromString:(NSString*)script;

// Evaluates the user-entered JavaScript in the WebView and returns the result.
// Will return nil if the web view is currently not available.
- (NSString*)stringByEvaluatingUserJavaScriptFromString:(NSString*)script;

// Checks if the document is loaded, and if so triggers any necessary logic.
- (void)checkDocumentLoaded;

// Returns a new autoreleased UIWebView.
- (UIWebView*)createWebView;

// Sets value to web view property.
- (void)setWebView:(UIWebView*)webView;

// Simulates the events generated by core.js during document loading process.
// Used for non-HTML documents (e.g. PDF) that do not support data flow from
// JavaScript to obj-c via iframe injection.
- (void)generateMissingDocumentLifecycleEvents;

// Makes a best-effort attempt to retroactively construct a load request for an
// observed-but-unexpected navigation. Should be called any time a page
// change is detected as having happened without the current internal state
// indicating it was expected.
- (void)generateMissingLoadRequestWithURL:(const GURL&)currentURL
                                 referrer:(const web::Referrer&)referrer;

// Returns a child scripting CRWWebController with the given window name.
- (id<CRWWebControllerScripting>)scriptingInterfaceForWindowNamed:
    (NSString*)name;

// Called when UIMoviePlayerControllerDidEnterFullscreenNotification is posted.
- (void)moviePlayerDidEnterFullscreen:(NSNotification*)notification;

// Called when UIMoviePlayerControllerDidExitFullscreenNotification is posted.
- (void)moviePlayerDidExitFullscreen:(NSNotification*)notification;

// Exits fullscreen mode for any playing videos.
- (void)exitFullscreenVideo;

// Handles presentation of the web document. Checks and handles URL changes,
// page refreshes, and title changes.
- (void)documentPresent;

// Handles queued JS to ObjC messages.
// All commands are passed via JSON strings, including parameters.
- (void)respondToJSInvoke;

// Pauses (|throttle|=YES) or resumes (|throttle|=NO) crwebinvoke message
// processing.
- (void)setJsMessageQueueThrottled:(BOOL)throttle;

// Removes document load commands from the queue. Otherwise they could be
// handled after a new page load has begun, which would cause an unwanted
// redirect.
- (void)removeDocumentLoadCommandsFromQueue;

// Returns YES if the given URL has a scheme associated with JS->native calls.
- (BOOL)urlSchemeIsWebInvoke:(const GURL&)url;

// Returns window name given a message and its context.
- (NSString*)windowNameFromMessage:(base::DictionaryValue*)message
                           context:(NSDictionary*)context;

// Handles 'anchor.click' message.
- (BOOL)handleAnchorClickMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context;
// Handles 'document.loaded' message.
- (BOOL)handleDocumentLoadedMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context;
// Handles 'document.present' message.
- (BOOL)handleDocumentPresentMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context;
// Handles 'document.retitled' message.
- (BOOL)handleDocumentRetitledMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context;
// Handles 'window.close' message.
- (BOOL)handleWindowCloseMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context;
// Handles 'window.document.write' message.
- (BOOL)handleWindowDocumentWriteMessage:(base::DictionaryValue*)message
                                 context:(NSDictionary*)context;
// Handles 'window.location' message.
- (BOOL)handleWindowLocationMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context;
// Handles 'window.open' message.
- (BOOL)handleWindowOpenMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context;
// Handles 'window.stop' message.
- (BOOL)handleWindowStopMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context;
// Handles 'window.unload' message.
- (BOOL)handleWindowUnloadMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context;
@end

namespace {

// Utility to help catch unwanted JavaScript re-entries. An instance should
// be created on the stack any time JS will be executed.
// It uses an instance variable (passed in as a pointer to boolean) that needs
// to be initialized to false.
class ScopedReentryGuard {
 public:
  explicit ScopedReentryGuard(BOOL* is_inside_javascript_context)
      : is_inside_javascript_context_(is_inside_javascript_context) {
    DCHECK(!*is_inside_javascript_context_);
    *is_inside_javascript_context_ = YES;
  }
  ~ScopedReentryGuard() {
    DCHECK(*is_inside_javascript_context_);
    *is_inside_javascript_context_ = NO;
  }

 private:
  BOOL* is_inside_javascript_context_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedReentryGuard);
};

// Class allowing to selectively enable caching of |currentURL| on a
// CRWUIWebViewWebController. As long as an instance of this class lives,
// the CRWUIWebViewWebController passed as parameter will cache the result for
// |currentURL| calls.
class ScopedCachedCurrentUrl {
 public:
  explicit ScopedCachedCurrentUrl(CRWUIWebViewWebController* web_controller)
      : web_controller_(web_controller),
        had_cached_current_url_([web_controller urlCachingEnabled]) {
    if (!had_cached_current_url_)
      [web_controller_ setURLCachingEnabled:YES];
  }

  ~ScopedCachedCurrentUrl() {
    if (!had_cached_current_url_)
      [web_controller_ setURLCachingEnabled:NO];
  }

 private:
  CRWUIWebViewWebController* web_controller_;
  bool had_cached_current_url_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedCachedCurrentUrl);
};

// Normalizes the URL for the purposes of identifying the origin page (remove
// any parameters, fragments, etc.) and return an absolute string of the URL.
std::string NormalizedUrl(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  GURL page_url(url.ReplaceComponents(replacements));

  return page_url.spec();
}

// The maximum size of JSON message passed from JavaScript to ObjC.
// 256kB is an arbitrary number that was chosen to be a magnitude larger than
// any legitimate message.
const size_t kMaxMessageQueueSize = 262144;

}  // namespace

@implementation CRWUIWebViewWebController

- (instancetype)initWithWebState:(scoped_ptr<web::WebStateImpl>)webState {
  self = [super initWithWebState:webState.Pass()];
  if (self) {
    _jsInvokeParameterQueue.reset([[CRWJSInvokeParameterQueue alloc] init]);

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(moviePlayerDidEnterFullscreen:)
                          name:
        @"UIMoviePlayerControllerDidEnterFullscreenNotification"
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(moviePlayerDidExitFullscreen:)
                          name:
        @"UIMoviePlayerControllerDidExitFullscreenNotification"
                        object:nil];
    _recurringTaskDelegate = self;

    // UIWebViews require a redirect network client in order to accurately
    // detect server redirects.
    scoped_refptr<web::RequestTrackerImpl> requestTracker =
        self.webStateImpl->GetRequestTracker();
    if (requestTracker) {
      redirect_client_factory_.reset(
          [[CRWRedirectNetworkClientFactory alloc] initWithDelegate:self]);
      requestTracker->PostIOTask(
          base::Bind(&net::RequestTracker::AddNetworkClientFactory,
                     requestTracker, redirect_client_factory_));
    }
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

#pragma mark - CRWWebController public method implementations

- (void)loadCompleteWithSuccess:(BOOL)loadSuccess {
  [super loadCompleteWithSuccess:loadSuccess];
  [self removeDocumentLoadCommandsFromQueue];
}

- (BOOL)keyboardDisplayRequiresUserAction {
  return [_uiWebView keyboardDisplayRequiresUserAction];
}

- (void)setKeyboardDisplayRequiresUserAction:(BOOL)requiresUserAction {
  [_uiWebView setKeyboardDisplayRequiresUserAction:requiresUserAction];
}

- (void)evaluateUserJavaScript:(NSString*)script {
  [self setUserInteractionRegistered:YES];
  // A script which contains alert() call executed by UIWebView from gcd block
  // freezes the app (crbug.com/444106), hence this uses the NSObject API.
  [_uiWebView performSelectorOnMainThread:
      @selector(stringByEvaluatingJavaScriptFromString:)
                               withObject:script
                            waitUntilDone:NO];
}

#pragma mark Overridden public methods

- (void)wasShown {
  if (_uiWebView) {
    // Turn the timer back on, and do an immediate check for anything missed
    // while the timer was off.
    [self.recurringTaskDelegate runRecurringTask];
    _continuousCheckTimer->Reset();
  }
  [super wasShown];
}

- (void)wasHidden {
  if (self.webView) {
    // Turn the timer off, to cut down on work being done by background tabs.
    _continuousCheckTimer->Stop();
  }
  [super wasHidden];

  // The video player is not quit/dismissed when the home button is pressed and
  // Chrome is backgrounded (crbug.com/277206).
  if (_inFullscreenVideo)
    [self exitFullscreenVideo];
}


- (void)close {
  [super close];

  // The timers must not exist at this point, otherwise this object will leak.
  DCHECK(!_continuousCheckTimer);
  DCHECK(!_lowerFrequencyTimer);
}

- (void)childWindowClosed:(NSString*)windowName {
  // Get the substring of the window name after the hash.
  NSRange range = [windowName rangeOfString:
      base::SysUTF8ToNSString(web::kWindowNameSeparator)];
  if (range.location != NSNotFound) {
    NSString* target = [windowName substringFromIndex:(range.location + 1)];
    [self stringByEvaluatingJavaScriptFromString:
        [NSString stringWithFormat:@"__gCrWeb.windowClosed('%@');", target]];
  }
}

#pragma mark -
#pragma mark Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  [super injectWebViewContentView:webViewContentView];
  [self setWebView:static_cast<UIWebView*>(webViewContentView.webView)];
}

#pragma mark CRWJSInjectionEvaluatorMethods

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  NSString* safeScript = [self scriptByAddingWindowIDCheckForScript:script];
  web::EvaluateJavaScript(_uiWebView, safeScript, handler);
}

- (web::WebViewType)webViewType {
  return web::UI_WEB_VIEW_TYPE;
}

#pragma mark - Protected property implementations

- (UIView*)webView {
  return _uiWebView.get();
}

- (UIScrollView*)webScrollView {
  return [_uiWebView scrollView];
}

- (BOOL)urlCachingEnabled {
  return _urlCachingEnabled;
}

- (void)setURLCachingEnabled:(BOOL)enabled {
  if (enabled == _urlCachingEnabled)
    return;
  _urlCachingEnabled = enabled;
  _cachedURL.first = GURL();
}

- (BOOL)ignoreURLVerificationFailures {
  return _spoofableRequest;
}

- (NSString*)title {
  return [self stringByEvaluatingJavaScriptFromString:
      @"document.title.length ? document.title : ''"];
}

#pragma mark Protected method implementations

- (void)ensureWebViewCreated {
  if (!_uiWebView) {
    [self setWebView:[self createWebView]];
    // Notify super class about created web view. -webViewDidChange is not
    // called from -setWebView: as the latter used in unit tests with fake
    // web view, which cannot be added to view hierarchy.
    [self webViewDidChange];
  }
}

- (void)resetWebView {
  [self setWebView:nil];
}

- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  if (_cachedURL.first.is_valid()) {
    *trustLevel = _cachedURL.second;
    return _cachedURL.first;
  }
  ScopedReentryGuard reentryGuard(&_inJavaScriptContext);
  const GURL url =
      [CRWURLVerifyingProtocolHandler currentURLForWebView:_uiWebView
                                                trustLevel:trustLevel];

  // If verification succeeded, or the URL has changed, then the UIWebView is no
  // longer in the initial state.
  if (*trustLevel != web::URLVerificationTrustLevel::kNone ||
      url != GURL(url::kAboutBlankURL))
    _spoofableRequest = NO;

  if (*trustLevel == web::URLVerificationTrustLevel::kAbsolute) {
    _lastCorrectURLTime = base::TimeTicks::Now();
    if (self.urlCachingEnabled) {
      _cachedURL.first = url;
      _cachedURL.second = *trustLevel;
    }
  } else if (*trustLevel == web::URLVerificationTrustLevel::kNone &&
             (base::TimeTicks::Now() - _lastCorrectURLTime) <
                 base::TimeDelta::FromMilliseconds(
                     web::kContinuousCheckIntervalMSLow)) {
    // URL is not trusted, but the last time it was trusted is within
    // kContinuousCheckIntervalMSLow.
    *trustLevel = web::URLVerificationTrustLevel::kMixed;
  }

  return url;
}

- (void)registerUserAgent {
  web::BuildAndRegisterUserAgentForUIWebView(
      self.webStateImpl->GetRequestGroupID(),
      [self useDesktopUserAgent]);
}

- (BOOL)isCurrentNavigationItemPOST {
  DCHECK([self currentSessionEntry]);
  NSData* currentPOSTData =
      [self currentSessionEntry].navigationItemImpl->GetPostData();
  return currentPOSTData != nil;
}

// The core.js cannot pass messages back to obj-c  if it is injected
// to |WEB_VIEW_DOCUMENT| because it does not support iframe creation used
// by core.js to communicate back. That functionality is only supported
// by |WEB_VIEW_HTML_DOCUMENT|. |WEB_VIEW_DOCUMENT| is used when displaying
// non-HTML contents (e.g. PDF documents).
- (web::WebViewDocumentType)webViewDocumentType {
  // This happens during tests.
  if (!_uiWebView) {
    return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
  }
  NSString* documentType =
      [_uiWebView stringByEvaluatingJavaScriptFromString:
          @"'' + document"];
  if ([documentType isEqualToString:@"[object HTMLDocument]"])
    return web::WEB_VIEW_DOCUMENT_TYPE_HTML;
  else if ([documentType isEqualToString:@"[object Document]"])
    return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
  return web::WEB_VIEW_DOCUMENT_TYPE_UNKNOWN;
}

- (void)loadRequest:(NSMutableURLRequest*)request {
  DCHECK(web::GetWebClient());
  GURL requestURL = net::GURLWithNSURL(request.URL);
  // If the request is for WebUI, add information to let the network stack
  // access the requestGroupID.
  if (web::GetWebClient()->IsAppSpecificURL(requestURL)) {
    // Sub requests of a chrome:// page will not contain the user agent.
    // Instead use the username part of the URL to allow the network stack to
    // associate a request to the correct tab.
    request.URL = web::AddRequestGroupIDToURL(
        request.URL, self.webStateImpl->GetRequestGroupID());
  }
  [_uiWebView loadRequest:request];
}

- (void)loadWebHTMLString:(NSString*)html forURL:(const GURL&)URL {
  [_uiWebView loadHTMLString:html baseURL:net::NSURLWithGURL(URL)];
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  NSString* beaconCheckScript = [NSString stringWithFormat:
      @"try { typeof %@; } catch (e) { 'undefined'; }", beacon];
  NSString* result =
      [self stringByEvaluatingJavaScriptFromString:beaconCheckScript];
  return [result isEqualToString:@"object"];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Skip evaluation if there's no content (e.g., if what's being injected is
  // an umbrella manager).
  if ([script length]) {
    [super injectScript:script forClass:JSInjectionManagerClass];
    [self stringByEvaluatingJavaScriptFromString:script];
  }
}

- (void)willLoadCurrentURLInWebView {
#if !defined(NDEBUG)
  if (_uiWebView) {
    // This code uses non-documented API, but is not compiled in release.
    id documentView = [_uiWebView valueForKey:@"documentView"];
    id webView = [documentView valueForKey:@"webView"];
    NSString* userAgent = [webView performSelector:@selector(userAgentForURL:)
                                        withObject:nil];
    DCHECK(userAgent);
    const bool wrongRequestGroupID =
        ![self.webStateImpl->GetRequestGroupID()
            isEqualToString:web::ExtractRequestGroupIDFromUserAgent(userAgent)];
    DLOG_IF(ERROR, wrongRequestGroupID) << "Incorrect user agent in UIWebView";
  }
#endif  // !defined(NDEBUG)
}

- (void)loadRequestForCurrentNavigationItem {
  DCHECK(self.webView && !self.nativeController);
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  ProceduralBlock GETBlock = ^{
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    [self loadRequest:request];
  };

  // If there is no POST data, load the request as a GET right away.
  DCHECK([self currentSessionEntry]);
  web::NavigationItemImpl* currentItem =
      [self currentSessionEntry].navigationItemImpl;
  NSData* POSTData = currentItem->GetPostData();
  if (!POSTData) {
    GETBlock();
    return;
  }

  ProceduralBlock POSTBlock = ^{
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:[self currentHTTPHeaders]];
    [self registerLoadRequest:[self currentNavigationURL]
                     referrer:[self currentSessionEntryReferrer]
                   transition:[self currentTransition]];
    [self loadRequest:request];
  };

  // If POST data is empty or the user does not need to confirm,
  // load the request right away.
  if (!POSTData.length || currentItem->ShouldSkipResubmitDataConfirmation()) {
    POSTBlock();
    return;
  }

  // Prompt the user to confirm the POST request.
  [self.delegate webController:self
      onFormResubmissionForRequest:request
                     continueBlock:POSTBlock
                       cancelBlock:GETBlock];
}

- (void)setPageChangeProbability:(web::PageChangeProbability)probability {
  if (probability == web::PAGE_CHANGE_PROBABILITY_LOW) {
    // Reduce check interval to precautionary frequency.
    [self setContinuousCheckTimerInterval:
        base::TimeDelta::FromMilliseconds(web::kContinuousCheckIntervalMSLow)];
  } else {
    // Increase the timer frequency, as a window change is anticipated shortly.
    [self setContinuousCheckTimerInterval:
        base::TimeDelta::FromMilliseconds(web::kContinuousCheckIntervalMSHigh)];

    if (probability != web::PAGE_CHANGE_PROBABILITY_VERY_HIGH) {
      // The timer frequency is automatically lowered after a set duration in
      // case the guess was wrong, to avoid wedging in high-frequency mode.
      base::Closure closure = base::BindBlock(^{
          [self setContinuousCheckTimerInterval:
              base::TimeDelta::FromMilliseconds(
                  web::kContinuousCheckIntervalMSLow)];
      });
      _lowerFrequencyTimer.reset(
          new base::Timer(FROM_HERE,
                          base::TimeDelta::FromMilliseconds(
                              web::kContinuousCheckHighFrequencyMSMaxDuration),
                          closure, false /* not repeating */));
      _lowerFrequencyTimer->Reset();
    }
  }
}

- (BOOL)checkForUnexpectedURLChange {
    // The check makes no sense without an active web view.
  if (!self.webView)
    return NO;

  // Change to UIWebView default page is not considered a 'real' change and
  // URL changes are not reported.
  if ([self isDefaultPage])
    return NO;

  // Check if currentURL is unexpected (not the incoming page).
  // This is necessary to notice page changes if core.js injection is disabled
  // by a malicious page.
  if (!self.URLOnStartLoading.is_empty() &&
      [self currentURL] == self.URLOnStartLoading) {
    return NO;
  }

  // If the URL has changed, handle page load mechanics.
  ScopedCachedCurrentUrl scopedCurrentURL(self);
  [self webPageChanged];
  [self checkDocumentLoaded];
  [self titleDidChange];

  return YES;
}

- (void)abortWebLoad {
  // Current load will not complete; this should be communicated upstream to
  // the delegate, and flagged in the WebView so further messages can be
  // prevented (which may be confused for messages from newer pages).
  [_uiWebView stringByEvaluatingJavaScriptFromString:
      @"document._cancelled = true;"];
  [_uiWebView stopLoading];
}

- (void)resetLoadState {
  _loadCount = 0;
  _unloadCount = 0;
}

- (void)setSuppressDialogsWithHelperScript:(NSString*)script {
  [self stringByEvaluatingJavaScriptFromString:script];
}

- (void)checkDocumentLoaded {
  NSString* loaded =
      [self stringByEvaluatingJavaScriptFromString:
           @"document.readyState === 'loaded' || "
            "document.readyState === 'complete'"];
  if ([loaded isEqualToString:@"true"]) {
    [self didFinishNavigation];
  }
}

- (NSString*)currentReferrerString {
  return [self stringByEvaluatingJavaScriptFromString:@"document.referrer"];
}

- (void)titleDidChange {
  if (![self.delegate respondsToSelector:
           @selector(webController:titleDidChange:)]) {
    return;
  }

  // Checking the URL trust level is expensive. For performance reasons, the
  // current URL and the trust level cache must be enabled.
  // NOTE: Adding a ScopedCachedCurrentUrl here is not the right way to solve
  // this DCHECK.
  DCHECK(self.urlCachingEnabled);

  // Change to UIWebView default page is not considered a 'real' change and
  // title changes are not reported.
  if ([self isDefaultPage])
    return;

  // The title can be retrieved from the document only if the URL can be
  // trusted.
  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  [self currentURLWithTrustLevel:&trustLevel];
  if (trustLevel != web::URLVerificationTrustLevel::kAbsolute)
    return;

  NSString* title = self.title;
  if ([title length])
    [self.delegate webController:self titleDidChange:title];
}

- (void)teminateNetworkActivity {
  [super terminateNetworkActivity];
  _jsInvokeParameterQueue.reset([[CRWJSInvokeParameterQueue alloc] init]);
}

- (void)fetchWebPageSizeWithCompletionHandler:(void(^)(CGSize))handler {
  if (!self.webView) {
    handler(CGSizeZero);
    return;
  }

  // Ensure that JavaScript has been injected.
  [self.recurringTaskDelegate runRecurringTask];
  [super fetchWebPageSizeWithCompletionHandler:handler];
}

- (void)documentPresent {
  if (self.loadPhase != web::PAGE_LOADED &&
      self.loadPhase != web::LOAD_REQUESTED) {
    return;
  }

  ScopedCachedCurrentUrl scopedCurrentURL(self);

  // This is a good time to check if the URL has changed.
  BOOL urlChanged = [self checkForUnexpectedURLChange];

  // This is a good time to check if the page has refreshed.
  if (!urlChanged && self.windowId != self.lastSeenWindowID)
    [self webPageChanged];

  // Set initial title.
  [self titleDidChange];
}

- (void)webPageChanged {
  if (self.loadPhase != web::LOAD_REQUESTED ||
      self.lastRegisteredRequestURL.is_empty() ||
      self.lastRegisteredRequestURL != [self currentURL]) {
    // The page change was unexpected (not already messaged to
    // webWillStartLoadingURL), so fill in the load request.
    [self generateMissingLoadRequestWithURL:[self currentURL]
                                   referrer:[self currentReferrer]];
  }

  [super webPageChanged];
}

- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState {
  // A UIWebView's scroll view uses zoom scales in a non-standard way.  The
  // scroll view's |zoomScale| property is always equal to 1.0, and the
  // |minimumZoomScale| and |maximumZoomScale| properties are adjusted
  // proportionally to reflect the relative zoom scale.  Setting the |zoomScale|
  // property here scales the page by the value set (i.e. setting zoomScale to
  // 2.0 will update the zoom to twice its initial scale). The maximum-scale or
  // minimum-scale meta tags of a page may have changed since the state was
  // recorded, so clamp the zoom scale to the current range if necessary.
  DCHECK(zoomState.IsValid());
  CGFloat zoomScale = zoomState.IsLegacyFormat()
                          ? zoomState.zoom_scale()
                          : self.webScrollView.minimumZoomScale /
                                zoomState.minimum_zoom_scale();
  if (zoomScale < self.webScrollView.minimumZoomScale)
    zoomScale = self.webScrollView.minimumZoomScale;
  if (zoomScale > self.webScrollView.maximumZoomScale)
    zoomScale = self.webScrollView.maximumZoomScale;
  self.webScrollView.zoomScale = zoomScale;
}

- (void)handleCancelledError:(NSError*)error {
  // NSURLErrorCancelled errors generated by the Chrome net stack should be
  // aborted.  If the error was generated by the UIWebView, it will not have
  // an underlying net error and will be automatically retried by the web view.
  DCHECK_EQ(error.code, NSURLErrorCancelled);
  NSError* underlyingError = base::ios::GetFinalUnderlyingErrorFromError(error);
  NSString* netDomain = base::SysUTF8ToNSString(net::kErrorDomain);
  BOOL shouldAbortLoadForCancelledError =
      [underlyingError.domain isEqualToString:netDomain];
  if (!shouldAbortLoadForCancelledError)
    return;

  // NSURLCancelled errors with underlying errors are generated from the
  // Chrome network stack.  Abort the load in this case.
  [self abortLoad];

  switch (underlyingError.code) {
    case net::ERR_ABORTED:
      // |NSURLErrorCancelled| errors with underlying net error code
      // |net::ERR_ABORTED| are used by the Chrome network stack to
      // indicate that the current load should be aborted and the pending
      // entry should be discarded.
      [[self sessionController] discardNonCommittedEntries];
      break;
    case net::ERR_BLOCKED_BY_CLIENT:
      // |NSURLErrorCancelled| errors with underlying net error code
      // |net::ERR_BLOCKED_BY_CLIENT| are used by the Chrome network stack
      // to indicate that the current load should be aborted and the pending
      // entry should be kept.
      break;
    default:
      NOTREACHED();
  }
}

#pragma mark - JS to ObjC messaging

- (void)respondToJSInvoke {
  // This call is asynchronous. If the web view has been removed, there is
  // nothing left to do, so just discard the queued messages and return.
  if (!self.webView) {
    _jsInvokeParameterQueue.reset([[CRWJSInvokeParameterQueue alloc] init]);
    return;
  }
  // Messages are queued and processed asynchronously. However, user
  // may initiate JavaScript at arbitrary times (e.g. through Omnibox
  // "javascript:alert('foo')"). This delays processing of queued messages
  // until JavaScript execution is completed. See crbug/228125 for details.
  if (_inJavaScriptContext) {
    [self performSelector:@selector(respondToJSInvoke)
               withObject:nil
               afterDelay:0];
    return;
  }
  DCHECK(_jsInvokeParameterQueue);
  while (![_jsInvokeParameterQueue isEmpty]) {
    CRWJSInvokeParameters* parameters =
        [_jsInvokeParameterQueue popInvokeParameters];
    if (!parameters)
      return;
    // TODO(stuartmorgan): Some messages (e.g., window.write) should be
    // processed even if the page has already changed by the time they are
    // received. crbug.com/228275
    if ([parameters windowId] != [self windowId]) {
      // If there is a windowID mismatch, the document has been changed since
      // messages were added to the queue. Ignore the incoming messages.
      DLOG(WARNING) << "Messages from JS ignored due to non-matching windowID: "
                    << [[parameters windowId] UTF8String]
                    << " != " << [[self windowId] UTF8String];
      continue;
    }
    if (![self respondToMessageQueue:[parameters commandString]
                   userIsInteracting:[parameters userIsInteracting]
                           originURL:[parameters originURL]]) {
      DLOG(WARNING) << "Messages from JS not handled due to invalid format";
    }
  }
}

- (void)handleWebInvokeURL:(const GURL&)url request:(NSURLRequest*)request {
  DCHECK([self urlSchemeIsWebInvoke:url]);
  NSURL* nsurl = request.URL;
  // TODO(stuartmorgan): Remove the NSURL usage here. Will require a logic
  // change since GURL doesn't parse non-standard URLs into host and fragment
  if (![nsurl.host isEqualToString:[self windowId]]) {
    // If there is a windowID mismatch, we may be under attack from a
    // malicious page, so a defense is to reset the page.
    DLOG(WARNING) << "Messages from JS ignored due to non-matching windowID: "
                  << nsurl.host << " != " << [[self windowId] UTF8String];
    DLOG(WARNING) << "Page reset as security precaution";
    [self performSelector:@selector(reload) withObject:nil afterDelay:0];
    return;
  }
  if (url.spec().length() > kMaxMessageQueueSize) {
    DLOG(WARNING) << "Messages from JS ignored due to excessive length";
    return;
  }
  NSString* commandString = [[nsurl fragment]
      stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

  GURL originURL(net::GURLWithNSURL(request.mainDocumentURL));

  if (url.SchemeIs("crwebinvokeimmediate")) {
    [self respondToMessageQueue:commandString
              userIsInteracting:[self userIsInteracting]
                      originURL:originURL];
  } else {
    [_jsInvokeParameterQueue addCommandString:commandString
                            userIsInteracting:[self userIsInteracting]
                                    originURL:originURL
                                  forWindowId:[super windowId]];
    if (!_jsMessageQueueThrottled) {
      [self performSelector:@selector(respondToJSInvoke)
                 withObject:nil
                 afterDelay:0];
    }
  }
}

- (void)setJsMessageQueueThrottled:(BOOL)throttle {
  _jsMessageQueueThrottled = throttle;
  if (!throttle)
    [self respondToJSInvoke];
}

- (void)removeDocumentLoadCommandsFromQueue {
  [_jsInvokeParameterQueue removeCommandString:@"document.present"];
  [_jsInvokeParameterQueue removeCommandString:@"document.loaded"];
}

- (BOOL)urlSchemeIsWebInvoke:(const GURL&)url {
  return url.SchemeIs("crwebinvoke") || url.SchemeIs("crwebinvokeimmediate");
}

- (CRWJSInvokeParameterQueue*)jsInvokeParameterQueue {
  return _jsInvokeParameterQueue;
}

- (BOOL)respondToMessageQueue:(NSString*)messageQueue
            userIsInteracting:(BOOL)userIsInteracting
                    originURL:(const GURL&)originURL {
  ScopedCachedCurrentUrl scopedCurrentURL(self);

  int errorCode = 0;
  std::string errorMessage;
  scoped_ptr<base::Value> inputJSONData(base::JSONReader::ReadAndReturnError(
      base::SysNSStringToUTF8(messageQueue), false, &errorCode, &errorMessage));
  if (errorCode) {
    DLOG(WARNING) << "JSON parse error: %s" << errorMessage.c_str();
    return NO;
  }
  // MessageQueues pass messages as a list.
  base::ListValue* messages = nullptr;
  if (!inputJSONData->GetAsList(&messages)) {
    DLOG(WARNING) << "Message queue not a list";
    return NO;
  }
  for (size_t idx = 0; idx != messages->GetSize(); ++idx) {
    // The same-origin check has to be done for every command to mitigate the
    // risk of command sequences where the first command would change the page
    // and the subsequent commands would have unlimited access to it.
    if (originURL.GetOrigin() != self.currentURL.GetOrigin()) {
      DLOG(WARNING) << "Message source URL origin: " << originURL.GetOrigin()
                    << " does not match current URL origin: "
                    << self.currentURL.GetOrigin();
      return NO;
    }

    base::DictionaryValue* message = nullptr;
    if (!messages->GetDictionary(idx, &message)) {
      DLOG(WARNING) << "Message could not be retrieved";
      return NO;
    }
    BOOL messageHandled = [self respondToMessage:message
                               userIsInteracting:userIsInteracting
                                       originURL:originURL];
    if (!messageHandled)
      return NO;

    // If handling the message caused this page to be closed, stop processing
    // messages.
    // TODO(stuartmorgan): Ideally messages should continue to be handled until
    // the end of the event loop (e.g., window.close(); window.open(...);
    // should do both things). That would require knowing which messages came
    // in the same event loop, however.
    if ([self isBeingDestroyed])
      return YES;
  }
  return YES;
}

- (SEL)selectorToHandleJavaScriptCommand:(const std::string&)command {
  static std::map<std::string, SEL>* handlers = nullptr;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    handlers = new std::map<std::string, SEL>();
    (*handlers)["anchor.click"] = @selector(handleAnchorClickMessage:context:);
    (*handlers)["document.loaded"] =
        @selector(handleDocumentLoadedMessage:context:);
    (*handlers)["document.present"] =
        @selector(handleDocumentPresentMessage:context:);
    (*handlers)["document.retitled"] =
        @selector(handleDocumentRetitledMessage:context:);
    (*handlers)["window.close"] = @selector(handleWindowCloseMessage:context:);
    (*handlers)["window.document.write"] =
        @selector(handleWindowDocumentWriteMessage:context:);
    (*handlers)["window.location"] =
        @selector(handleWindowLocationMessage:context:);
    (*handlers)["window.open"] = @selector(handleWindowOpenMessage:context:);
    (*handlers)["window.stop"] = @selector(handleWindowStopMessage:context:);
    (*handlers)["window.unload"] =
        @selector(handleWindowUnloadMessage:context:);
  });
  DCHECK(handlers);
  auto iter = handlers->find(command);
  return iter != handlers->end()
             ? iter->second
             : [super selectorToHandleJavaScriptCommand:command];
}

- (NSString*)windowNameFromMessage:(base::DictionaryValue*)message
                           context:(NSDictionary*)context {
  std::string target;
  if(!message->GetString("target", &target)) {
    DLOG(WARNING) << "JS message parameter not found: target";
    return nil;
  }
  DCHECK(&target);
  DCHECK(context[web::kOriginURLKey]);
  const GURL& originURL = net::GURLWithNSURL(context[web::kOriginURLKey]);

  // Unique string made for page/target combination.
  // Safe to delimit unique string with # since page references won't
  // contain #.
  return base::SysUTF8ToNSString(
      NormalizedUrl(originURL) + web::kWindowNameSeparator + target);
}

#pragma mark -
#pragma mark JavaScript message handlers

- (BOOL)handleAnchorClickMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context {
  // Reset the external click request.
  [self resetExternalRequest];

  std::string href;
  if (!message->GetString("href", &href)) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return NO;
  }
  const GURL targetURL(href);
  const GURL currentURL([self currentURL]);
  if (currentURL != targetURL) {
    if (web::UrlHasWebScheme(targetURL)) {
      // The referrer is not known yet, and will be updated later.
      const web::Referrer emptyReferrer;
      [self registerLoadRequest:targetURL
                       referrer:emptyReferrer
                     transition:ui::PAGE_TRANSITION_LINK];
      [self setPageChangeProbability:web::PAGE_CHANGE_PROBABILITY_HIGH];
    } else if (web::GetWebClient()->IsAppSpecificURL(targetURL) &&
               web::GetWebClient()->IsAppSpecificURL(currentURL)) {
      // Allow navigations between app-specific URLs
      [self removeWebViewAllowingCachedReconstruction:NO];
      ui::PageTransition pageTransitionLink =
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
      const web::Referrer referrer(currentURL, web::ReferrerPolicyDefault);
      web::WebState::OpenURLParams openParams(targetURL, referrer, CURRENT_TAB,
                                              pageTransitionLink, true);
      [self.delegate openURLWithParams:openParams];
    }
  }
  return YES;
}

- (BOOL)handleDocumentLoadedMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context {
  // Very early hashchange events can be missed, hence this extra explicit
  // check.
  [self checkForUnexpectedURLChange];
  [self didFinishNavigation];
  return YES;
}

- (BOOL)handleDocumentPresentMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  NSString* documentCancelled =
      [self stringByEvaluatingJavaScriptFromString:@"document._cancelled"];
  if (![documentCancelled isEqualToString:@"true"])
    [self documentPresent];
  return YES;
}

- (BOOL)handleDocumentRetitledMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  [self titleDidChange];
  return YES;
}

- (BOOL)handleWindowCloseMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context {
  NSString* windowName = [self windowNameFromMessage:message
                                             context:context];
  if (!windowName)
    return NO;
  [[self scriptingInterfaceForWindowNamed:windowName] orderClose];
  return YES;
}

- (BOOL)handleWindowDocumentWriteMessage:(base::DictionaryValue*)message
                                 context:(NSDictionary*)context {
  NSString* windowName = [self windowNameFromMessage:message
                                             context:context];
  if (!windowName)
    return NO;
  std::string HTML;
  if (!message->GetString("html", &HTML)) {
    DLOG(WARNING) << "JS message parameter not found: html";
    return NO;
  }
  [[self scriptingInterfaceForWindowNamed:windowName]
      loadHTML:base::SysUTF8ToNSString(HTML)];
  return YES;
}

- (BOOL)handleWindowLocationMessage:(base::DictionaryValue*)message
                            context:(NSDictionary*)context {
  NSString* windowName = [self windowNameFromMessage:message
                                             context:context];
  if (!windowName)
    return NO;
  std::string command;
  if (!message->GetString("command", &command)) {
    DLOG(WARNING) << "JS message parameter not found: command";
    return NO;
  }
  std::string value;
  if (!message->GetString("value", &value)) {
    DLOG(WARNING) << "JS message parameter not found: value";
    return NO;
  }
  std::string escapedValue;
  base::EscapeJSONString(value, true, &escapedValue);
  NSString* HTML =
      [NSString stringWithFormat:@"<script>%s = %s;</script>",
                                 command.c_str(),
                                 escapedValue.c_str()];
  [[self scriptingInterfaceForWindowNamed:windowName] loadHTML:HTML];
  return YES;
}

- (BOOL)handleWindowOpenMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context {
  NSString* windowName = [self windowNameFromMessage:message
                                             context:context];
  if (!windowName)
    return NO;
  std::string targetURL;
  if (!message->GetString("url", &targetURL)) {
    DLOG(WARNING) << "JS message parameter not found: url";
    return NO;
  }
  std::string referrerPolicy;
  if (!message->GetString("referrerPolicy", &referrerPolicy)) {
    DLOG(WARNING) << "JS message parameter not found: referrerPolicy";
    return NO;
  }
  GURL resolvedURL = targetURL.empty() ?
      self.defaultURL :
      GURL(net::GURLWithNSURL(context[web::kOriginURLKey])).Resolve(targetURL);
  DCHECK(&resolvedURL);
  web::NewWindowInfo
      windowInfo(resolvedURL,
                 windowName,
                 [self referrerPolicyFromString:referrerPolicy],
                 [context[web::kUserIsInteractingKey] boolValue]);

  [self openPopupWithInfo:windowInfo];
  return YES;
}

- (BOOL)handleWindowStopMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context {
  NSString* windowName = [self windowNameFromMessage:message
                                             context:context];
  if (!windowName)
    return NO;
  [[self scriptingInterfaceForWindowNamed:windowName] stopLoading];
  return YES;
}

- (BOOL)handleWindowUnloadMessage:(base::DictionaryValue*)message
                          context:(NSDictionary*)context {
  [self setPageChangeProbability:web::PAGE_CHANGE_PROBABILITY_VERY_HIGH];
  return YES;
}

#pragma mark Private methods

- (BOOL)isDefaultPage {
  if ([[self stringByEvaluatingJavaScriptFromString:@"document._defaultPage"]
       isEqualToString:@"true"]) {
    return self.currentURL == self.defaultURL;
  }
  return NO;
}

- (BOOL)isLinkNavigation:(UIWebViewNavigationType)navigationType {
  switch (navigationType) {
    case UIWebViewNavigationTypeLinkClicked:
      return YES;
    case UIWebViewNavigationTypeOther:
      return [self userClickedRecently];
    default:
      return NO;
  }
}

- (void)setContinuousCheckTimerInterval:(const base::TimeDelta&)interval {
  // The timer should never be set when there's no web view.
  DCHECK(_uiWebView);

  BOOL shouldStartTimer =
      !_continuousCheckTimer.get() || _continuousCheckTimer->IsRunning();
  base::Closure closure = base::BindBlock(^{
      // Only perform JS checks if CRWWebController is not already in JavaScript
      // context. This is possible when "javascript:..." is executed from
      // Omnibox and this block is run from the timer.
      if (!_inJavaScriptContext)
        [self.recurringTaskDelegate runRecurringTask];
  });
  _continuousCheckTimer.reset(
      new base::Timer(FROM_HERE, interval, closure, true));
  if (shouldStartTimer)
    _continuousCheckTimer->Reset();
  if (_lowerFrequencyTimer &&
      interval == base::TimeDelta::FromMilliseconds(
          web::kContinuousCheckIntervalMSLow)) {
    _lowerFrequencyTimer.reset();
  }
}

- (NSString*)stringByEvaluatingJavaScriptFromString:(NSString*)script {
  if (!_uiWebView)
    return nil;

  ScopedReentryGuard reentryGuard(&_inJavaScriptContext);
  return [_uiWebView stringByEvaluatingJavaScriptFromString:script];
}

- (NSString*)stringByEvaluatingUserJavaScriptFromString:(NSString*)script {
  [self setUserInteractionRegistered:YES];
  return [self stringByEvaluatingJavaScriptFromString:script];
}

- (UIWebView*)createWebView {
  UIWebView* webView = web::CreateWebView(
      CGRectZero,
      self.webStateImpl->GetRequestGroupID(),
      [self useDesktopUserAgent]);

  // Mark the document object of the default page as such, so that it is not
  // mistaken for a 'real' page by change detection mechanisms.
  [webView stringByEvaluatingJavaScriptFromString:
      @"document._defaultPage = true;"];

  [webView setScalesPageToFit:YES];
  // Turn off data-detectors. MobileSafari does the same thing.
  [webView setDataDetectorTypes:UIDataDetectorTypeNone];

  return [webView autorelease];
}

- (void)setWebView:(UIWebView*)webView {
  DCHECK_NE(_uiWebView.get(), webView);
  // Per documentation, must clear the delegate before releasing the UIWebView
  // to avoid errant dangling pointers.
  [_uiWebView setDelegate:nil];
  _uiWebView.reset([webView retain]);
  [_uiWebView setDelegate:self];
  // Clear out the trusted URL cache.
  _lastCorrectURLTime = base::TimeTicks();
  _cachedURL.first = GURL();
  // Reset the spoofable state (see declaration comment).
  // TODO(stuartmorgan): Fix the fact that there's no guarantee that no
  // navigation has happened before the UIWebView is set here (ideally by
  // unifying the creation and setting flow).
  _spoofableRequest = YES;

  if (webView) {
    _inJavaScriptContext = NO;
    // Do initial injection even before loading another page, since the window
    // object is re-used.
    [self injectEarlyInjectionScripts];
  } else {
    _continuousCheckTimer.reset();
    // This timer exists only to change the frequency of the main timer, so it
    // should not outlive the main timer.
    _lowerFrequencyTimer.reset();
  }
}

- (void)generateMissingDocumentLifecycleEvents {
  // The webView can be removed between this method being queued and invoked.
  if (!self.webView)
    return;
  if ([self webViewDocumentType] == web::WEB_VIEW_DOCUMENT_TYPE_GENERIC) {
    [self documentPresent];
    [self didFinishNavigation];
  }
}

- (void)generateMissingLoadRequestWithURL:(const GURL&)currentURL
                                 referrer:(const web::Referrer&)referrer {
  [self loadCancelled];
  // Initialize transition based on whether the request is user-initiated or
  // not. This is a best guess to replace lost transition type informationj.
  ui::PageTransition transition = self.userInteractionRegistered
                                      ? ui::PAGE_TRANSITION_LINK
                                      : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  // If the URL agrees with session state, use the session's transition.
  if (currentURL == [self currentNavigationURL]) {
    transition = [self currentTransition];
  }

  [self registerLoadRequest:currentURL referrer:referrer transition:transition];
}

- (id<CRWWebControllerScripting>)scriptingInterfaceForWindowNamed:
    (NSString*)name {
  if (![self.delegate respondsToSelector:
        @selector(webController:scriptingInterfaceForWindowNamed:)]) {
    return nil;
  }
  return [self.delegate webController:self
     scriptingInterfaceForWindowNamed:name];
}

#pragma mark FullscreenVideo

- (void)moviePlayerDidEnterFullscreen:(NSNotification*)notification {
  _inFullscreenVideo = YES;
}

- (void)moviePlayerDidExitFullscreen:(NSNotification*)notification {
  _inFullscreenVideo = NO;
}

- (void)exitFullscreenVideo {
  [self stringByEvaluatingJavaScriptFromString:
      @"__gCrWeb.exitFullscreenVideo();"];
}

#pragma mark -
#pragma mark CRWRecurringTaskDelegate

// Checks for page changes are made continuously.
- (void)runRecurringTask {
  if (!self.webView)
    return;

  [self injectEarlyInjectionScripts];
  [self checkForUnexpectedURLChange];
}

#pragma mark -
#pragma mark CRWRedirectClientDelegate

- (void)wasRedirectedToRequest:(NSURLRequest*)request
              redirectResponse:(NSURLResponse*)response {
  // This callback can be received after -close is called; ignore it.
  if (self.isBeingDestroyed)
    return;

  // Register the redirected load request if it originated from the main page
  // load.
  GURL redirectedURL = net::GURLWithNSURL(response.URL);
  if ([self currentNavigationURL] == redirectedURL) {
    [self registerLoadRequest:net::GURLWithNSURL(request.URL)
                     referrer:[self currentReferrer]
                   transition:ui::PAGE_TRANSITION_SERVER_REDIRECT];
  }
}

#pragma mark -
#pragma mark UIWebViewDelegate Methods

// Called when a load begins, and for subsequent subpages.
- (BOOL)webView:(UIWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(UIWebViewNavigationType)navigationType {
  DVLOG(5) << "webViewShouldStartLoadWithRequest "
           << net::FormatUrlRequestForLogging(request);

  if (self.isBeingDestroyed)
    return NO;

  GURL url = net::GURLWithNSURL(request.URL);

  // The crwebnull protocol is used where an element requires a URL but it
  // should not trigger any activity on the WebView.
  if (url.SchemeIs("crwebnull"))
    return NO;

  if ([self urlSchemeIsWebInvoke:url]) {
    [self handleWebInvokeURL:url request:request];
    return NO;
  }

  // ##### IMPORTANT NOTE #####
  // Do not add new code above this line unless you're certain about what you're
  // doing with respect to JS re-entry.
  ScopedReentryGuard javaScriptReentryGuard(&_inJavaScriptContext);
  web::FrameInfo* targetFrame = nullptr;  // No reliable way to get this info.
  BOOL isLinkClick = [self isLinkNavigation:navigationType];
  return [self shouldAllowLoadWithRequest:request
                              targetFrame:targetFrame
                              isLinkClick:isLinkClick];
}

// Called at multiple points during a load, such as at the start of loading a
// page, and every time an iframe loads. Not called again for server-side
// redirects.
- (void)webViewDidStartLoad:(UIWebView *)webView {
  NSURLRequest* request = webView.request;
  DVLOG(5) << "webViewDidStartLoad "
           << net::FormatUrlRequestForLogging(request);
  // |webView:shouldStartLoad| may not be called or called with different URL
  // and mainDocURL for the request in certain page navigations. There
  // are at least 2 known page navigations where this occurs, in these cases it
  // is imperative the URL verification timer is started here.
  // The 2 known cases are:
  // 1) A malicious page suppressing core.js injection and calling
  //    window.history.back() or window.history.forward()
  // 2) An iframe loading a URL using target=_blank.
  // TODO(shreyasv): crbug.com/349155. Understand further why this happens
  // in some case and not in others.
  if (webView != self.webView) {
    // This happens sometimes as tests are brought down.
    // TODO(jimblackler): work out why and fix the problem at source.
    LOG(WARNING) << " UIWebViewDelegate message received for inactive WebView.";
    return;
  }
  DCHECK(!self.isBeingDestroyed);
  // Increment the number of pending loads. This will be balanced by either
  // a |-webViewDidFinishLoad:| or |-webView:didFailLoadWithError:|.
  ++_loadCount;
  [self.recurringTaskDelegate runRecurringTask];
}

// Called when the page (or one of its subframes) finishes loading. This is
// called multiple times during a page load, with varying frequency depending
// on the action (going back, loading a page with frames, redirecting).
// See http://goto/efrmm for a summary of why this is so painful.
- (void)webViewDidFinishLoad:(UIWebView*)webView {
  DVLOG(5) << "webViewDidFinishLoad "
           << net::FormatUrlRequestForLogging(webView.request);
  DCHECK(!self.isHalted);
  // Occasionally this delegate is invoked as a side effect during core.js
  // injection. It is necessary to ensure we do not attempt to start the
  // injection process a second time.
  if (!_inJavaScriptContext)
    [self.recurringTaskDelegate runRecurringTask];

  [self performSelector:@selector(generateMissingDocumentLifecycleEvents)
             withObject:nil
             afterDelay:0];

  ++_unloadCount;
  if ((_loadCount == _unloadCount) && (self.loadPhase != web::LOAD_REQUESTED))
    [self checkDocumentLoaded];
}

// Called when there is an error loading the page. Some errors aren't actual
// errors, but are caused by user actions such as stopping a page load
// prematurely.
- (void)webView:(UIWebView*)webView didFailLoadWithError:(NSError*)error {
  DVLOG(5) << "webViewDidFailLoadWithError "
           << net::FormatUrlRequestForLogging(webView.request);
  ++_unloadCount;

  // Under unknown circumstances navigation item can be null. In that case the
  // state of web/ will not be valid and app will crash. Early return avoid a
  // crash (crbug.com/411912).
  if (!self.webStateImpl ||
      !self.webStateImpl->GetNavigationManagerImpl().GetVisibleItem()) {
    return;
  }

  // There's no reliable way to know if a load is for the main frame, so make a
  // best-effort guess.
  // |_loadCount| is reset to 0 before starting loading a new page, and is
  // incremented in each call to |-webViewDidStartLoad:|. The main request
  // is the first one to be loaded, and thus has a |_loadCount| of 1.
  // Sub-requests have a |_loadCount| > 1.
  // An iframe loading after the main page also has a |_loadCount| of 1, as
  // |_loadCount| is reset at the end of the main page load. In that case,
  // |loadPhase_| is web::PAGE_LOADED (as opposed to web::PAGE_LOADING for a
  // main request).
  const bool isMainFrame = (_loadCount == 1 &&
                            self.loadPhase != web::PAGE_LOADED);
  [self handleLoadError:error inMainFrame:isMainFrame];
}

#pragma mark -
#pragma mark Testing methods

-(id<CRWRecurringTaskDelegate>)recurringTaskDelegate {
  return _recurringTaskDelegate;
}

@end
