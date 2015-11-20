// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/crw_test_js_injection_receiver.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"

@interface CRWTestUIWebViewEvaluator : NSObject<CRWJSInjectionEvaluator> {
  base::scoped_nsobject<UIWebView> _webView;
}
@end

@implementation CRWTestUIWebViewEvaluator

- (id) init {
  if (self = [super init])
    _webView.reset([[UIWebView alloc] init]);
  return self;
}

#pragma mark -
#pragma mark CRWJSInjectionEvaluatorMethods

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  base::WeakNSObject<CRWTestUIWebViewEvaluator> weakEvaluator(self);
  dispatch_async(dispatch_get_main_queue(), ^{
      UIWebView* webView = weakEvaluator ? weakEvaluator.get()->_webView : nil;
      NSString* result =
          [webView stringByEvaluatingJavaScriptFromString:script];
      if (handler)
        handler(result, nil);
  });
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  NSString* result = [_webView stringByEvaluatingJavaScriptFromString:
      [NSString stringWithFormat:@"typeof %@", beacon]];
  return [result isEqualToString:@"object"];
}

- (void)injectScript:(NSString*)script
            forClass:(Class)jsInjectionManagerClass {
  // Web layer guarantees that __gCrWeb object is always injected first.
  [_webView stringByEvaluatingJavaScriptFromString:@"window.__gCrWeb = {};"];
  [_webView stringByEvaluatingJavaScriptFromString:script];
}

- (web::WebViewType)webViewType {
  return web::UI_WEB_VIEW_TYPE;
}

@end

// TODO(crbug.com/486840): Replace CRWTestUIWebViewEvaluator with
// CRWTestWKWebViewEvaluator in CRWTestJSInjectionReceiver once UIWebView is
// no longer used.
@interface CRWTestWKWebViewEvaluator : NSObject<CRWJSInjectionEvaluator> {
  // Web view for JavaScript evaluation.
  base::scoped_nsobject<WKWebView> _webView;
  // Set to track injected script managers.
  base::scoped_nsobject<NSMutableSet> _injectedScriptManagers;
}
@end

@implementation CRWTestWKWebViewEvaluator

- (instancetype)init {
  if (self = [super init]) {
    _webView.reset([[WKWebView alloc] init]);
    _injectedScriptManagers.reset([[NSMutableSet alloc] init]);
  }
  return self;
}

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  [_webView evaluateJavaScript:script completionHandler:handler];
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)JSInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  return [_injectedScriptManagers containsObject:JSInjectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Web layer guarantees that __gCrWeb object is always injected first.
  NSString* supplementedScript =
      [@"window.__gCrWeb = {};" stringByAppendingString:script];
  [_webView evaluateJavaScript:supplementedScript completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

- (web::WebViewType)webViewType {
  return web::WK_WEB_VIEW_TYPE;
}

@end

@interface CRWTestJSInjectionReceiver () {
  base::scoped_nsobject<CRWTestUIWebViewEvaluator> evaluator_;
}
@end

@implementation CRWTestJSInjectionReceiver

- (id)init {
  base::scoped_nsobject<CRWTestUIWebViewEvaluator> evaluator(
      [[CRWTestUIWebViewEvaluator alloc] init]);
  if (self = [super initWithEvaluator:evaluator])
    evaluator_.swap(evaluator);
  return self;
}

@end
