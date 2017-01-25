// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/criwv_web_view_impl.h"

#include <memory>
#include <utility>

#import "base/ios/weak_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "ios/web_view/internal/criwv_browser_state.h"
#import "ios/web_view/internal/translate/criwv_translate_client.h"
#import "ios/web_view/public/criwv_web_view_delegate.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@interface CRIWVWebViewImpl ()<CRWWebDelegate, CRWWebStateDelegate> {
  id<CRIWVWebViewDelegate> _delegate;
  ios_web_view::CRIWVBrowserState* _browserState;
  std::unique_ptr<web::WebStateImpl> _webStateImpl;
  base::WeakNSObject<CRWWebController> _webController;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;

  CGFloat _loadProgress;
}

@end

@implementation CRIWVWebViewImpl

@synthesize delegate = _delegate;
@synthesize loadProgress = _loadProgress;

- (instancetype)initWithBrowserState:
    (ios_web_view::CRIWVBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _webStateImpl = base::MakeUnique<web::WebStateImpl>(_browserState);
    _webStateImpl->GetNavigationManagerImpl().InitializeSession(nil, nil, NO,
                                                                0);
    _webStateDelegate = base::MakeUnique<web::WebStateDelegateBridge>(self);
    _webStateImpl->SetDelegate(_webStateDelegate.get());
    _webController.reset(_webStateImpl->GetWebController());
    [_webController setDelegate:self];
    [_webController setWebUsageEnabled:YES];

    // Initialize Translate.
    ios_web_view::CRIWVTranslateClient::CreateForWebState(_webStateImpl.get());
  }
  return self;
}

- (UIView*)view {
  return [_webController view];
}

- (BOOL)canGoBack {
  return _webStateImpl && _webStateImpl->GetNavigationManager()->CanGoBack();
}

- (BOOL)canGoForward {
  return _webStateImpl && _webStateImpl->GetNavigationManager()->CanGoForward();
}

- (BOOL)isLoading {
  return _webStateImpl->IsLoading();
}

- (NSURL*)visibleURL {
  return net::NSURLWithGURL(_webStateImpl->GetVisibleURL());
}

- (NSString*)pageTitle {
  return base::SysUTF16ToNSString(_webStateImpl->GetTitle());
}

- (void)goBack {
  if (_webStateImpl->GetNavigationManager())
    _webStateImpl->GetNavigationManager()->GoBack();
}

- (void)goForward {
  if (_webStateImpl->GetNavigationManager())
    _webStateImpl->GetNavigationManager()->GoForward();
}

- (void)reload {
  [_webController reload];
}

- (void)stopLoading {
  [_webController stopLoading];
}

- (void)loadURL:(NSURL*)URL {
  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(URL));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  [_webController loadWithParams:params];
}

- (void)evaluateJavaScript:(NSString*)javaScriptString
         completionHandler:(void (^)(id, NSError*))completionHandler {
  [_webStateImpl->GetJSInjectionReceiver() executeJavaScript:javaScriptString
                                           completionHandler:completionHandler];
}

- (void)setDelegate:(id<CRIWVWebViewDelegate>)delegate {
  _delegate = delegate;

  // Set up the translate delegate.
  ios_web_view::CRIWVTranslateClient* translateClient =
      ios_web_view::CRIWVTranslateClient::FromWebState(_webStateImpl.get());
  id<CRIWVTranslateDelegate> translateDelegate = nil;
  if ([_delegate respondsToSelector:@selector(translateDelegate)])
    translateDelegate = [_delegate translateDelegate];
  translateClient->set_translate_delegate(translateDelegate);
}

// -----------------------------------------------------------------------
// WebDelegate implementation.

- (void)notifyDidUpdateWithChanges:(CRIWVWebViewUpdateType)changes {
  SEL selector = @selector(webView:didUpdateWithChanges:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate webView:self didUpdateWithChanges:changes];
  }
}

- (void)webController:(CRWWebController*)webController
       titleDidChange:(NSString*)title {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeTitle];
}

- (void)webDidUpdateSessionForLoadWithParams:
            (const web::NavigationManager::WebLoadParams&)params
                        wasInitialNavigation:(BOOL)initialNavigation {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webWillFinishHistoryNavigationFromEntry:(CRWSessionEntry*)fromEntry {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webDidAddPendingURL {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webDidUpdateHistoryStateWithPageURL:(const GURL&)pageUrl {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webCancelStartLoadingRequest {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webDidStartLoadingURL:(const GURL&)currentUrl
          shouldUpdateHistory:(BOOL)updateHistory {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webDidFinishWithURL:(const GURL&)url loadSuccess:(BOOL)loadSuccess {
  SEL selector = @selector(webView:didFinishLoadingWithURL:loadSuccess:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate webView:self
        didFinishLoadingWithURL:net::NSURLWithGURL(url)
                    loadSuccess:loadSuccess];
  }
}

- (void)webLoadCancelled:(const GURL&)url {
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeURL];
}

- (void)webWillAddPendingURL:(const GURL&)url
                  transition:(ui::PageTransition)transition {
}
- (CRWWebController*)webPageOrderedOpen:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             windowName:(NSString*)windowName
                           inBackground:(BOOL)inBackground {
  return nil;
}

- (CRWWebController*)webPageOrderedOpen {
  return nil;
}

- (void)webPageOrderedClose {
}
- (void)openURLWithParams:(const web::WebState::OpenURLParams&)params {
}
- (BOOL)openExternalURL:(const GURL&)url linkClicked:(BOOL)linkClicked {
  return NO;
}
- (void)webController:(CRWWebController*)webController
    retrievePlaceholderOverlayImage:(void (^)(UIImage*))block {
}
- (void)webController:(CRWWebController*)webController
    onFormResubmissionForRequest:(NSURLRequest*)request
                   continueBlock:(ProceduralBlock)continueBlock
                     cancelBlock:(ProceduralBlock)cancelBlock {
}
- (void)webWillReload {
}
- (void)webWillInitiateLoadWithParams:
    (web::NavigationManager::WebLoadParams&)params {
}
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  SEL selector = @selector(webView:shouldOpenURL:mainDocumentURL:linkClicked:);
  if ([_delegate respondsToSelector:selector]) {
    return [_delegate webView:self
                shouldOpenURL:net::NSURLWithGURL(url)
              mainDocumentURL:net::NSURLWithGURL(mainDocumentURL)
                  linkClicked:linkClicked];
  }
  return YES;
}

// -----------------------------------------------------------------------
// CRWWebStateDelegate implementation.

- (void)webState:(web::WebState*)webState didChangeProgress:(double)progress {
  _loadProgress = progress;
  [self notifyDidUpdateWithChanges:CRIWVWebViewUpdateTypeProgress];
}

@end
