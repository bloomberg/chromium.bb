// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/crw_test_js_injection_receiver.h"

#import <UIKit/UIKit.h>

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
  dispatch_async(dispatch_get_main_queue(), ^{
      // TODO(shreyasv): Change to weaknsobject once weaknsobject is moved to
      // ios/base.
      NSString* result =
          [_webView stringByEvaluatingJavaScriptFromString:script];
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
  [_webView stringByEvaluatingJavaScriptFromString:script];
}

- (web::WebViewType)webViewType {
  return web::UI_WEB_VIEW_TYPE;
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
