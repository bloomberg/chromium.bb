// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_web_view_navigation_observer.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/http_response_headers_util.h"
#include "ios/web/common/features.h"
#include "ios/web/common/url_util.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_web_view_navigation_observer_delegate.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_web_view_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::NavigationManagerImpl;

using web::wk_navigation_util::IsRestoreSessionUrl;
using web::wk_navigation_util::IsPlaceholderUrl;

@interface CRWWebViewNavigationObserver ()

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(weak, nonatomic, readonly) NSDictionary* WKWebViewObservers;

@property(nonatomic, assign, readonly) web::WebStateImpl* webStateImpl;

// The WKNavigationDelegate handler class.
@property(nonatomic, readonly, strong)
    CRWWKNavigationHandler* navigationHandler;

// The actual URL of the document object (i.e., the last committed URL).
@property(nonatomic, readonly) const GURL& documentURL;

// The NavigationManagerImpl associated with the web state.
@property(nonatomic, readonly) NavigationManagerImpl* navigationManagerImpl;

@end

@implementation CRWWebViewNavigationObserver

#pragma mark - Property

- (void)setWebView:(WKWebView*)webView {
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView removeObserver:self forKeyPath:keyPath];
  }

  _webView = webView;

  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
}

- (NSDictionary*)WKWebViewObservers {
  return @{
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"loading" : @"webViewLoadingStateDidChange",
    @"canGoForward" : @"webViewBackForwardStateDidChange",
    @"canGoBack" : @"webViewBackForwardStateDidChange"
  };
}

- (NavigationManagerImpl*)navigationManagerImpl {
  return self.webStateImpl ? &(self.webStateImpl->GetNavigationManagerImpl())
                           : nil;
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForNavigationObserver:self];
}

- (CRWWKNavigationHandler*)navigationHandler {
  return [self.delegate navigationHandlerForNavigationObserver:self];
}

- (const GURL&)documentURL {
  return [self.delegate documentURLForNavigationObserver:self];
}

#pragma mark - KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSString* dispatcherSelectorName = self.WKWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName) {
    // With ARC memory management, it is not known what a method called
    // via a selector will return. If a method returns a retained value
    // (e.g. NS_RETURNS_RETAINED) that returned object will leak as ARC is
    // unable to property insert the correct release calls for it.
    // All selectors used here return void and take no parameters so it's safe
    // to call a function mapping to the method implementation manually.
    SEL selector = NSSelectorFromString(dispatcherSelectorName);
    IMP methodImplementation = [self methodForSelector:selector];
    if (methodImplementation) {
      void (*methodCallFunction)(id, SEL) =
          reinterpret_cast<void (*)(id, SEL)>(methodImplementation);
      methodCallFunction(self, selector);
    }
  }
}

// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange {
  if (![self.delegate webViewIsBeingDestroyed:self]) {
    self.webStateImpl->SendChangeLoadProgress(self.webView.estimatedProgress);
  }
}

// Called when WKWebView loading state has been changed.
- (void)webViewLoadingStateDidChange {
  if (self.webView.loading)
    return;

  GURL webViewURL = net::GURLWithNSURL(self.webView.URL);

  if (![self.navigationHandler isCurrentNavigationBackForward])
    return;

  web::NavigationContextImpl* existingContext = [self.navigationHandler
      contextForPendingMainFrameNavigationWithURL:webViewURL];

  // When traversing history restored from a previous session, WKWebView does
  // not fire 'pageshow', 'onload', 'popstate' or any of the
  // WKNavigationDelegate callbacks for back/forward navigation from an about:
  // scheme placeholder URL to another entry or if either of the redirect fails
  // to load (e.g. in airplane mode). Loading state KVO is the only observable
  // event in this scenario, so force a reload to trigger redirect from
  // restore_session.html to the restored URL.
  bool previousURLHasAboutScheme =
      self.documentURL.SchemeIs(url::kAboutScheme) ||
      IsPlaceholderUrl(self.documentURL) ||
      web::GetWebClient()->IsAppSpecificURL(self.documentURL);
  bool is_back_forward_navigation =
      existingContext &&
      (existingContext->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK);
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      IsRestoreSessionUrl(webViewURL)) {
    if (previousURLHasAboutScheme || is_back_forward_navigation) {
      [self.webView reload];
      self.navigationHandler.navigationState =
          web::WKNavigationState::REQUESTED;
      return;
    }
  }

  // For failed navigations, WKWebView will sometimes revert to the previous URL
  // before committing the current navigation or resetting the web view's
  // |isLoading| property to NO.  If this is the first navigation for the web
  // view, this will result in an empty URL.
  BOOL navigationWasCommitted = self.navigationHandler.navigationState !=
                                web::WKNavigationState::REQUESTED;
  if (!navigationWasCommitted &&
      (webViewURL.is_empty() || webViewURL == self.documentURL)) {
    return;
  }

  if (!navigationWasCommitted &&
      !self.navigationHandler.pendingNavigationInfo.cancelled) {
    // A fast back-forward navigation does not call |didCommitNavigation:|, so
    // signal page change explicitly.
    DCHECK_EQ(self.documentURL.GetOrigin(), webViewURL.GetOrigin());
    BOOL isSameDocumentNavigation =
        [self.delegate navigationObserver:self
            isURLChangeSameDocumentNavigation:webViewURL];

    [self.delegate navigationObserver:self
                 didChangeDocumentURL:webViewURL
                           forContext:existingContext];
    if (!existingContext) {
      // This URL was not seen before, so register new load request.
      [self.delegate navigationObserver:self
                          didLoadNewURL:webViewURL
              forSameDocumentNavigation:isSameDocumentNavigation];
    } else {
      // Same document navigation does not contain response headers.
      net::HttpResponseHeaders* headers =
          isSameDocumentNavigation
              ? nullptr
              : self.webStateImpl->GetHttpResponseHeaders();
      existingContext->SetResponseHeaders(headers);
      existingContext->SetIsSameDocument(isSameDocumentNavigation);
      existingContext->SetHasCommitted(!isSameDocumentNavigation);
      self.webStateImpl->OnNavigationStarted(existingContext);
      [self.delegate navigationObserver:self
               didChangePageWithContext:existingContext];
      self.webStateImpl->OnNavigationFinished(existingContext);
    }
  }

  [self.delegate navigationObserverDidChangeSSLStatus:self];
  [self.delegate navigationObserver:self didFinishNavigation:existingContext];
}

// Called when WKWebView canGoForward/canGoBack state has been changed.
- (void)webViewBackForwardStateDidChange {
  // Don't trigger for LegacyNavigationManager because its back/foward state
  // doesn't always match that of WKWebView.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    self.webStateImpl->OnBackForwardStateChanged();
}

@end
