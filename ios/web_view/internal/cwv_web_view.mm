// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_internal.h"

#include <memory>
#include <utility>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "google_apis/google_api_keys.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/reload_type.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/cwv_html_element_internal.h"
#import "ios/web_view/internal/cwv_navigation_action_internal.h"
#import "ios/web_view/internal/cwv_scroll_view_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/web_view_global_state_util.h"
#import "ios/web_view/internal/web_view_java_script_dialog_presenter.h"
#import "ios/web_view/internal/web_view_web_state_policy_decider.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_preview_element_info.h"
#import "ios/web_view/public/cwv_ui_delegate.h"
#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A key used in NSCoder to store the session storage object.
NSString* const kSessionStorageKey = @"sessionStorage";
}

@interface CWVWebView ()<CRWWebStateDelegate, CRWWebStateObserver> {
  CWVWebViewConfiguration* _configuration;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ios_web_view::WebViewWebStatePolicyDecider>
      _webStatePolicyDecider;
  double _estimatedProgress;
  // Handles presentation of JavaScript dialogs.
  std::unique_ptr<ios_web_view::WebViewJavaScriptDialogPresenter>
      _javaScriptDialogPresenter;
}

// Redefine these properties as readwrite to define setters, which send KVO
// notifications.
@property(nonatomic, readwrite) double estimatedProgress;
@property(nonatomic, readwrite) BOOL canGoBack;
@property(nonatomic, readwrite) BOOL canGoForward;
@property(nonatomic, readwrite) NSURL* lastCommittedURL;
@property(nonatomic, readwrite) BOOL loading;
@property(nonatomic, readwrite, copy) NSString* title;
@property(nonatomic, readwrite) NSURL* visibleURL;

// Updates the availability of the back/forward navigation properties exposed
// through |canGoBack| and |canGoForward|.
- (void)updateNavigationAvailability;
// Updates the URLs exposed through |lastCommittedURL| and |visibleURL|.
- (void)updateCurrentURLs;
// Updates |title| property.
- (void)updateTitle;
// Returns a new CWVAutofillController created from |_webState|.
- (CWVAutofillController*)newAutofillController;
// Returns a new CWVTranslationController created from |_webState|.
- (CWVTranslationController*)newTranslationController;

@end

static NSString* gUserAgentProduct = nil;

@implementation CWVWebView

@synthesize autofillController = _autofillController;
@synthesize canGoBack = _canGoBack;
@synthesize canGoForward = _canGoForward;
@synthesize configuration = _configuration;
@synthesize estimatedProgress = _estimatedProgress;
@synthesize lastCommittedURL = _lastCommittedURL;
@synthesize loading = _loading;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize title = _title;
@synthesize translationController = _translationController;
@synthesize UIDelegate = _UIDelegate;
@synthesize scrollView = _scrollView;
@synthesize visibleURL = _visibleURL;

+ (void)initialize {
  if (self != [CWVWebView class]) {
    return;
  }

  ios_web_view::InitializeGlobalState();
}

+ (NSString*)userAgentProduct {
  return gUserAgentProduct;
}

+ (void)setUserAgentProduct:(NSString*)product {
  gUserAgentProduct = [product copy];
}

+ (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret {
  google_apis::SetAPIKey(base::SysNSStringToUTF8(googleAPIKey));

  std::string clientIDString = base::SysNSStringToUTF8(clientID);
  std::string clientSecretString = base::SysNSStringToUTF8(clientSecret);
  for (size_t i = 0; i < google_apis::CLIENT_NUM_ITEMS; ++i) {
    google_apis::OAuth2Client client =
        static_cast<google_apis::OAuth2Client>(i);
    google_apis::SetOAuth2ClientID(client, clientIDString);
    google_apis::SetOAuth2ClientSecret(client, clientSecretString);
  }
}

- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration {
  self = [super initWithFrame:frame];
  if (self) {
    _configuration = configuration;
    [_configuration registerWebView:self];
    _scrollView = [[CWVScrollView alloc] init];
    [self resetWebStateWithSessionStorage:nil];
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
  }
}

- (void)goBack {
  if (_webState->GetNavigationManager())
    _webState->GetNavigationManager()->GoBack();
}

- (void)goForward {
  if (_webState->GetNavigationManager())
    _webState->GetNavigationManager()->GoForward();
}

- (void)reload {
  // |check_for_repost| is false because CWVWebView does not support repost form
  // dialogs.
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            false /* check_for_repost */);
}

- (void)stopLoading {
  _webState->Stop();
}

- (void)loadRequest:(NSURLRequest*)request {
  DCHECK_EQ(nil, request.HTTPBodyStream)
      << "request.HTTPBodyStream is not supported.";

  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(request.URL));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers = [request.allHTTPHeaderFields copy];
  params.post_data = [request.HTTPBody copy];
  _webState->GetNavigationManager()->LoadURLWithParams(params);
  [self updateCurrentURLs];
}

- (void)evaluateJavaScript:(NSString*)javaScriptString
         completionHandler:(void (^)(id, NSError*))completionHandler {
  [_webState->GetJSInjectionReceiver() executeJavaScript:javaScriptString
                                       completionHandler:completionHandler];
}

- (void)setUIDelegate:(id<CWVUIDelegate>)UIDelegate {
  _UIDelegate = UIDelegate;

  _javaScriptDialogPresenter->SetUIDelegate(_UIDelegate);
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (newSuperview) {
    _webState->WasShown();
  } else {
    _webState->WasHidden();
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
}

- (void)webState:(web::WebState*)webState
    navigationItemsPruned:(size_t)pruned_item_count {
  [self updateCurrentURLs];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationAvailability];
  SEL selector = @selector(webViewDidStartProvisionalNavigation:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewDidStartProvisionalNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  if (details.is_in_page) {
    // Do not call webViewDidCommitNavigation: for fragment navigations.
    return;
  }

  if ([_navigationDelegate
          respondsToSelector:@selector(webViewDidCommitNavigation:)]) {
    [_navigationDelegate webViewDidCommitNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationAvailability];
  [self updateCurrentURLs];

  NSError* error = navigation->GetError();
  SEL selector = @selector(webView:didFailNavigationWithError:);
  if (error && [_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webView:self didFailNavigationWithError:error];
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState.get(), webState);
  if (!success) {
    // Failure callbacks will be handled inside |webState:didFinishNavigation:|.
    return;
  }
  SEL selector = @selector(webViewDidFinishNavigation:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewDidFinishNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  self.estimatedProgress = progress;
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  self.loading = _webState->IsLoading();
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  self.loading = _webState->IsLoading();
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateTitle];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  SEL selector = @selector(webViewWebContentProcessDidTerminate:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewWebContentProcessDidTerminate:self];
  }
}

- (void)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  SEL selector = @selector(webView:runContextMenuWithTitle:forHTMLElement:inView
                                  :userGestureLocation:);
  if (![_UIDelegate respondsToSelector:selector]) {
    return;
  }
  NSURL* hyperlink = net::NSURLWithGURL(params.link_url);
  NSURL* mediaSource = net::NSURLWithGURL(params.src_url);
  CWVHTMLElement* HTMLElement =
      [[CWVHTMLElement alloc] initWithHyperlink:hyperlink
                                    mediaSource:mediaSource
                                           text:params.link_text];
  [_UIDelegate webView:self
      runContextMenuWithTitle:params.menu_title
               forHTMLElement:HTMLElement
                       inView:params.view
          userGestureLocation:params.location];
}

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  SEL selector =
      @selector(webView:createWebViewWithConfiguration:forNavigationAction:);
  if (![_UIDelegate respondsToSelector:selector]) {
    return nullptr;
  }

  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:net::NSURLWithGURL(URL)];
  CWVNavigationAction* navigationAction =
      [[CWVNavigationAction alloc] initWithRequest:request
                                     userInitiated:initiatedByUser];
  CWVWebView* webView = [_UIDelegate webView:self
              createWebViewWithConfiguration:_configuration
                         forNavigationAction:navigationAction];
  if (!webView) {
    return nullptr;
  }
  web::WebState* webViewWebState = webView->_webState.get();
  webViewWebState->SetHasOpener(true);
  return webViewWebState;
}

- (void)closeWebState:(web::WebState*)webState {
  SEL selector = @selector(webViewDidClose:);
  if ([_UIDelegate respondsToSelector:selector]) {
    [_UIDelegate webViewDidClose:self];
  }
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  return _javaScriptDialogPresenter.get();
}

- (BOOL)webState:(web::WebState*)webState
    shouldPreviewLinkWithURL:(const GURL&)linkURL {
  SEL selector = @selector(webView:shouldPreviewElement:);
  if ([_UIDelegate respondsToSelector:selector]) {
    CWVPreviewElementInfo* elementInfo = [[CWVPreviewElementInfo alloc]
        initWithLinkURL:net::NSURLWithGURL(linkURL)];
    return [_UIDelegate webView:self shouldPreviewElement:elementInfo];
  }
  return NO;
}

- (UIViewController*)webState:(web::WebState*)webState
    previewingViewControllerForLinkWithURL:(const GURL&)linkURL {
  SEL selector = @selector(webView:previewingViewControllerForElement:);
  if ([_UIDelegate respondsToSelector:selector]) {
    CWVPreviewElementInfo* elementInfo = [[CWVPreviewElementInfo alloc]
        initWithLinkURL:net::NSURLWithGURL(linkURL)];
    return [_UIDelegate webView:self
        previewingViewControllerForElement:elementInfo];
  }
  return nil;
}

- (void)webState:(web::WebState*)webState
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  SEL selector = @selector(webView:commitPreviewingViewController:);
  if ([_UIDelegate respondsToSelector:selector]) {
    [_UIDelegate webView:self
        commitPreviewingViewController:previewingViewController];
  }
}

#pragma mark - Translation

- (CWVTranslationController*)translationController {
  if (!_translationController) {
    _translationController = [self newTranslationController];
  }
  return _translationController;
}

- (CWVTranslationController*)newTranslationController {
  ios_web_view::WebViewTranslateClient::CreateForWebState(_webState.get());
  ios_web_view::WebViewTranslateClient* translateClient =
      ios_web_view::WebViewTranslateClient::FromWebState(_webState.get());
  return [[CWVTranslationController alloc]
      initWithTranslateClient:translateClient];
}

#pragma mark - Autofill

- (CWVAutofillController*)autofillController {
  if (!_autofillController) {
    _autofillController = [self newAutofillController];
  }
  return _autofillController;
}

- (CWVAutofillController*)newAutofillController {
  AutofillAgent* autofillAgent = [[AutofillAgent alloc]
      initWithPrefService:_configuration.browserState->GetPrefs()
                 webState:_webState.get()];
  JsAutofillManager* JSAutofillManager =
      base::mac::ObjCCastStrict<JsAutofillManager>(
          [_webState->GetJSInjectionReceiver()
              instanceOfClass:[JsAutofillManager class]]);
  JsSuggestionManager* JSSuggestionManager =
      base::mac::ObjCCastStrict<JsSuggestionManager>(
          [_webState->GetJSInjectionReceiver()
              instanceOfClass:[JsSuggestionManager class]]);
  return [[CWVAutofillController alloc] initWithWebState:_webState.get()
                                           autofillAgent:autofillAgent
                                       JSAutofillManager:JSAutofillManager
                                     JSSuggestionManager:JSSuggestionManager];
}

#pragma mark - Preserving and Restoring State

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder {
  [super encodeRestorableStateWithCoder:coder];
  [coder encodeObject:_webState->BuildSessionStorage()
               forKey:kSessionStorageKey];
}

- (void)decodeRestorableStateWithCoder:(NSCoder*)coder {
  [super decodeRestorableStateWithCoder:coder];
  CRWSessionStorage* sessionStorage =
      [coder decodeObjectForKey:kSessionStorageKey];
  [self resetWebStateWithSessionStorage:sessionStorage];
}

#pragma mark - Private methods

// Creates a WebState instance and assigns it to |_webState|.
// It replaces the old |_webState| if any.
// The WebState is restored from |sessionStorage| if provided.
- (void)resetWebStateWithSessionStorage:
    (nullable CRWSessionStorage*)sessionStorage {
  if (_webState && _webState->GetView().superview == self) {
    if (_webStateObserver) {
      _webState->RemoveObserver(_webStateObserver.get());
    }

    // The web view provided by the old |_webState| has been added as a subview.
    // It must be removed and replaced with a new |_webState|'s web view, which
    // is added later.
    [_webState->GetView() removeFromSuperview];
  }

  web::WebState::CreateParams webStateCreateParams(_configuration.browserState);
  if (sessionStorage) {
    _webState = web::WebState::CreateWithStorageSession(webStateCreateParams,
                                                        sessionStorage);
  } else {
    _webState = web::WebState::Create(webStateCreateParams);
  }

  if (!_webStateObserver) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  _webState->AddObserver(_webStateObserver.get());

  _webStateDelegate = std::make_unique<web::WebStateDelegateBridge>(self);
  _webState->SetDelegate(_webStateDelegate.get());

  _webStatePolicyDecider =
      std::make_unique<ios_web_view::WebViewWebStatePolicyDecider>(
          _webState.get(), self);

  _javaScriptDialogPresenter =
      std::make_unique<ios_web_view::WebViewJavaScriptDialogPresenter>(self,
                                                                       nullptr);

  _scrollView.proxy = _webState.get()->GetWebViewProxy().scrollViewProxy;

  if (_translationController) {
    id<CWVTranslationControllerDelegate> delegate =
        _translationController.delegate;
    _translationController = [self newTranslationController];
    _translationController.delegate = delegate;
  }

  // Recreate and restore the delegate only if previously lazily loaded.
  if (_autofillController) {
    id<CWVAutofillControllerDelegate> delegate = _autofillController.delegate;
    _autofillController = [self newAutofillController];
    _autofillController.delegate = delegate;
  }

  [self addInternalWebViewAsSubview];

  [self updateNavigationAvailability];
  [self updateCurrentURLs];
  [self updateTitle];
  self.loading = NO;
  self.estimatedProgress = 0.0;
}

// Adds the web view provided by |_webState| as a subview unless it has already.
- (void)addInternalWebViewAsSubview {
  UIView* subview = _webState->GetView();
  if (subview.superview == self) {
    return;
  }
  subview.frame = self.bounds;
  subview.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addSubview:subview];
}

- (void)updateNavigationAvailability {
  self.canGoBack = _webState && _webState->GetNavigationManager()->CanGoBack();
  self.canGoForward =
      _webState && _webState->GetNavigationManager()->CanGoForward();
}

- (void)updateCurrentURLs {
  self.lastCommittedURL = net::NSURLWithGURL(_webState->GetLastCommittedURL());
  self.visibleURL = net::NSURLWithGURL(_webState->GetVisibleURL());
}

- (void)updateTitle {
  self.title = base::SysUTF16ToNSString(_webState->GetTitle());
}

#pragma mark - Internal Methods

- (void)shutDown {
  _webState.reset();
}

@end
