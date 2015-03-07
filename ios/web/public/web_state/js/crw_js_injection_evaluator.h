// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_EVALUATOR_H_
#define IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_EVALUATOR_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/web_view_type.h"

// The type of the completion handler block that is called from
// |evaluateJavaScript:completionHandler|
namespace web {
typedef void (^JavaScriptCompletion)(NSString*, NSError*);
}

@protocol CRWJSInjectionEvaluator

// Evaluates the supplied JavaScript in the WebView. Calls |completionHandler|
// with results of the evaluation (which may be nil if the implementing object
// has no way to run the evaluation or the evaluation returns a nil value)
// or an NSError if there is an error. The |completionHandler| can be nil.
- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler;

// Checks to see if the script for a class has been injected into the
// current page already, given the class and the script's presence
// beacon (a JS object that should exist iff the script has been injected).
- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon;

// Injects the given script into the current page on behalf of
// |jsInjectionManagerClass|. This should only be used for injecting
// the manager's script, and not for evaluating arbitrary JavaScript.
- (void)injectScript:(NSString*)script forClass:(Class)jsInjectionManagerClass;

// Returns type of web view used for evaluation. Must not change for the
// lifetime of the object.
- (web::WebViewType)webViewType;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_EVALUATOR_H_
