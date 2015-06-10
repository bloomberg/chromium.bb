// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_ui_simple_web_view_controller.h"

#include "base/ios/weak_nsobject.h"
#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@interface CRWUISimpleWebViewController () <UIWebViewDelegate>
@end

@implementation CRWUISimpleWebViewController {
  base::scoped_nsobject<UIWebView> _webView;
  base::WeakNSProtocol<id<CRWSimpleWebViewControllerDelegate>> _delegate;
}

- (instancetype)initWithUIWebView:(UIWebView*)webView {
  self = [super init];
  if (self) {
    DCHECK(webView);
    _webView.reset([webView retain]);
    [_webView setDelegate:self];
  }
  return self;
}

- (void)dealloc {
  [_webView setDelegate:nil];
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
  return [_webView stringByEvaluatingJavaScriptFromString:@"document.title"];
}

- (void)loadRequest:(NSURLRequest*)request {
  [_webView loadRequest:request];
}

- (void)loadHTMLString:(NSString*)html baseURL:(NSURL*)baseURL {
  [_webView loadHTMLString:html baseURL:baseURL];
}

- (void)setDelegate:(id<CRWSimpleWebViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (id<CRWSimpleWebViewControllerDelegate>)delegate {
  return _delegate.get();
}

- (void)loadPDFAtFilePath:(NSString*)filePath {
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:filePath]);
  NSData* data = [NSData dataWithContentsOfFile:filePath];
  [_webView loadData:data
              MIMEType:@"application/pdf"
      textEncodingName:@"utf-8"
               baseURL:[NSURL URLWithString:@""]];
}

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  web::EvaluateJavaScript(_webView, script, handler);
}

#pragma mark -
#pragma mark UIWebViewDelegateMethods

- (BOOL)webView:(UIWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(UIWebViewNavigationType)navigationType {
  if ([_delegate respondsToSelector:@selector(simpleWebViewController:
                                           shouldStartLoadWithRequest:)]) {
    return [_delegate simpleWebViewController:self
                   shouldStartLoadWithRequest:request];
  }
  // By default all loads are allowed.
  return YES;
}

- (void)webViewDidFinishLoad:(UIWebView*)webView {
  if (![_delegate respondsToSelector:@selector(simpleWebViewController:
                                                   titleMayHaveChanged:)]) {
    return;
  }
  [_delegate simpleWebViewController:self titleMayHaveChanged:self.title];
}

@end
