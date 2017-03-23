// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_web_view.h"

#include <memory>
#include <utility>

#import "base/ios/weak_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/reload_type.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web_view/internal/cwv_html_element_internal.h"
#import "ios/web_view/internal/cwv_navigation_action_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_java_script_dialog_presenter.h"
#import "ios/web_view/internal/web_view_web_state_policy_decider.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_ui_delegate.h"
#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

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

// Redefine the property as readwrite to define -setEstimatedProgress:, which
// can be used to send KVO notification.
@property(nonatomic, readwrite) double estimatedProgress;

@end

@implementation CWVWebView

@synthesize configuration = _configuration;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize translationDelegate = _translationDelegate;
@synthesize estimatedProgress = _estimatedProgress;
@synthesize UIDelegate = _UIDelegate;

- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration {
  self = [super initWithFrame:frame];
  if (self) {
    _configuration = [configuration copy];

    web::WebState::CreateParams webStateCreateParams(
        configuration.browserState);
    _webState = web::WebState::Create(webStateCreateParams);
    _webState->SetWebUsageEnabled(true);

    _webStateObserver =
        base::MakeUnique<web::WebStateObserverBridge>(_webState.get(), self);
    _webStateDelegate = base::MakeUnique<web::WebStateDelegateBridge>(self);
    _webState->SetDelegate(_webStateDelegate.get());

    _webStatePolicyDecider =
        base::MakeUnique<ios_web_view::WebViewWebStatePolicyDecider>(
            _webState.get(), self);

    _javaScriptDialogPresenter =
        base::MakeUnique<ios_web_view::WebViewJavaScriptDialogPresenter>(
            self, nullptr);

    // Initialize Translate.
    ios_web_view::WebViewTranslateClient::CreateForWebState(_webState.get());
  }
  return self;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  UIView* subview = _webState->GetView();
  if (subview.superview == self) {
    return;
  }
  subview.frame = self.frame;
  subview.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addSubview:subview];
}

- (BOOL)canGoBack {
  return _webState && _webState->GetNavigationManager()->CanGoBack();
}

- (BOOL)canGoForward {
  return _webState && _webState->GetNavigationManager()->CanGoForward();
}

- (BOOL)isLoading {
  return _webState->IsLoading();
}

- (NSURL*)visibleURL {
  return net::NSURLWithGURL(_webState->GetVisibleURL());
}

- (NSString*)title {
  return base::SysUTF16ToNSString(_webState->GetTitle());
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
  params.extra_headers.reset([request.allHTTPHeaderFields copy]);
  params.post_data.reset([request.HTTPBody copy]);
  _webState->GetNavigationManager()->LoadURLWithParams(params);
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

- (void)setTranslationDelegate:(id<CWVTranslateDelegate>)translationDelegate {
  _translationDelegate = translationDelegate;
  ios_web_view::WebViewTranslateClient::FromWebState(_webState.get())
      ->set_translate_delegate(translationDelegate);
}

// -----------------------------------------------------------------------
// WebStateObserver implementation.

- (void)webState:(web::WebState*)webState
    didStartProvisionalNavigationForURL:(const GURL&)URL {
  SEL selector = @selector(webViewDidStartProvisionalNavigation:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewDidStartProvisionalNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:(const web::LoadCommittedDetails&)details {
  if ([_navigationDelegate
          respondsToSelector:@selector(webViewDidCommitNavigation:)]) {
    [_navigationDelegate webViewDidCommitNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState.get(), webState);
  SEL selector = @selector(webView:didLoadPageWithSuccess:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webView:self didLoadPageWithSuccess:success];
  }
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  self.estimatedProgress = progress;
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  SEL selector = @selector(webViewWebContentProcessDidTerminate:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewWebContentProcessDidTerminate:self];
  }
}

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  SEL selector = @selector(webView:runContextMenuWithTitle:forHTMLElement:inView
                                  :userGestureLocation:);
  if (![_UIDelegate respondsToSelector:selector]) {
    return NO;
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
  return YES;
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
  // TODO(crbug.com/702298): Window created by CWVUIDelegate should be closable.
  CWVWebView* webView = [_UIDelegate webView:self
              createWebViewWithConfiguration:_configuration
                         forNavigationAction:navigationAction];
  if (!webView) {
    return nullptr;
  }
  return webView->_webState.get();
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

@end
