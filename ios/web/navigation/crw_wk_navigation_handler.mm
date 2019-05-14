// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWKNavigationHandler {
  // Used to poll for a SafeBrowsing warning being displayed. This is created in
  // |decidePolicyForNavigationAction| and destroyed once any of the following
  // happens: 1) a SafeBrowsing warning is detected; 2) any WKNavigationDelegate
  // method is called; 3) |stopLoading| is called.
  base::RepeatingTimer _safeBrowsingWarningDetectionTimer;
}

- (instancetype)init {
  if (self = [super init]) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
  }
  return self;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self didReceiveWKNavigationDelegateCallback];
}

#pragma mark - Private methods

// This method should be called on receiving WKNavigationDelegate callbacks. It
// will log a metric if the callback occurs after the reciever has already been
// closed. It also stops the SafeBrowsing warning detection timer, since after
// this point it's too late for a SafeBrowsing warning to be displayed for the
// navigation for which the timer was started.
- (void)didReceiveWKNavigationDelegateCallback {
  if ([self.delegate navigationHandlerWebViewBeingDestroyed:self]) {
    UMA_HISTOGRAM_BOOLEAN("Renderer.WKWebViewCallbackAfterDestroy", true);
  }
  _safeBrowsingWarningDetectionTimer.Stop();
}

#pragma mark - Public methods

- (void)stopLoading {
  self.pendingNavigationInfo.cancelled = YES;
  _safeBrowsingWarningDetectionTimer.Stop();
}

- (base::RepeatingTimer*)safeBrowsingWarningDetectionTimer {
  return &_safeBrowsingWarningDetectionTimer;
}

@end
