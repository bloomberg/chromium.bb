// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_test_js_injection_receiver.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"

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

- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)completionHandler {
  web::ExecuteJavaScript(_webView, script, completionHandler);
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_injectedScriptManagers containsObject:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Web layer guarantees that __gCrWeb object is always injected first.
  NSString* supplementedScript =
      [@"window.__gCrWeb = {};" stringByAppendingString:script];
  [_webView evaluateJavaScript:supplementedScript completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

@end

@interface CRWTestJSInjectionReceiver () {
  base::scoped_nsobject<CRWTestWKWebViewEvaluator> evaluator_;
}
@end

@implementation CRWTestJSInjectionReceiver

- (id)init {
  base::scoped_nsobject<CRWTestWKWebViewEvaluator> evaluator(
      [[CRWTestWKWebViewEvaluator alloc] init]);
  if (self = [super initWithEvaluator:evaluator])
    evaluator_.swap(evaluator);
  return self;
}

@end
