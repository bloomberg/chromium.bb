// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_simple_web_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@interface CRWWKSimpleWebViewController () <WKNavigationDelegate>
// Consults the delegate to decide whether a load request should be started.
- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request;
@end

@implementation CRWWKSimpleWebViewController {
  base::scoped_nsobject<WKWebView> _webView;
  base::WeakNSProtocol<id<CRWSimpleWebViewControllerDelegate>> _delegate;
}

- (instancetype)initWithWKWebView:(WKWebView*)webView {
  self = [super init];
  if (self) {
    DCHECK(webView);
    _webView.reset([webView retain]);
    [_webView setNavigationDelegate:self];
    [_webView addObserver:self forKeyPath:@"title" options:0 context:NULL];
  }
  return self;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"title"];
  [super dealloc];
}

#pragma mark -
#pragma mark CRWSimpleWebView implementation

- (void)reload {
  [_webView reload];
}

- (UIView*)view {
  return _webView.get();
}

- (UIScrollView*)scrollView {
  return [_webView scrollView];
}

- (NSString*)title {
  return [_webView title];
}

- (void)loadRequest:(NSURLRequest*)request {
  [_webView loadRequest:request];
}

- (void)loadHTMLString:(NSString*)html baseURL:(NSURL*)baseURL {
  [_webView loadHTMLString:html baseURL:baseURL];
}

- (void)loadPDFAtFilePath:(NSString*)filePath {
  if (!base::ios::IsRunningOnIOS9OrLater()) {
    // WKWebView on iOS 8 can't load local files, so just bail.
    NOTIMPLEMENTED();
    return;
  }
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:filePath]);
  NSURL* fileURL = [NSURL fileURLWithPath:filePath];
  DCHECK(fileURL);
  [_webView loadFileURL:fileURL allowingReadAccessToURL:fileURL];
}

- (void)setDelegate:(id<CRWSimpleWebViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (id<CRWSimpleWebViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  web::EvaluateJavaScript(_webView, script, handler);
}

#pragma mark -
#pragma mark KVO callback

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqualToString:@"title"]);
  if ([_delegate respondsToSelector:@selector(simpleWebViewController:
                                                  titleMayHaveChanged:)]) {
    [_delegate simpleWebViewController:self titleMayHaveChanged:[self title]];
  }
}

#pragma mark -
#pragma mark Delegate forwarding methods

- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request {
  if ([_delegate respondsToSelector:@selector(simpleWebViewController:
                                           shouldStartLoadWithRequest:)]) {
    return [_delegate simpleWebViewController:self
                   shouldStartLoadWithRequest:request];
  }
  // By default all loads are allowed.
  return YES;
}

#pragma mark -
#pragma mark WKNavigationDelegate methods

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  decisionHandler([self shouldStartLoadWithRequest:navigationAction.request]
                      ? WKNavigationActionPolicyAllow
                      : WKNavigationActionPolicyCancel);
}

@end
