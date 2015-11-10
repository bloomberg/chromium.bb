// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace {

// Converts result of WKWebView script evaluation to UIWebView format.
NSString* UIResultFromWKResult(id result) {
  if (!result)
    return @"";

  CFTypeID result_type = CFGetTypeID(result);
  if (result_type == CFStringGetTypeID())
    return result;

  if (result_type == CFNumberGetTypeID())
    return [result stringValue];

  if (result_type == CFBooleanGetTypeID())
    return [result boolValue] ? @"true" : @"false";

  if (result_type == CFNullGetTypeID())
    return @"";

  // TODO(stuartmorgan): Stringify other types.
  NOTREACHED();
  return nil;
}

}  // namespace

namespace web {

void EvaluateJavaScript(UIWebView* web_view,
                        NSString* script,
                        JavaScriptCompletion completion_handler) {
  base::WeakNSObject<UIWebView> weak_web_view(web_view);
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* result =
        [weak_web_view stringByEvaluatingJavaScriptFromString:script];
    if (completion_handler)
      completion_handler(result, nil);
  });
}

void EvaluateJavaScript(WKWebView* web_view,
                        NSString* script,
                        JavaScriptCompletion completion_handler) {
  DCHECK([script length]);
  void (^web_view_completion_handler)(id, NSError*) = nil;
  // Do not create a web_view_completion_handler if no |completion_handler| is
  // passed to this function. WKWebView guarantees to call all completion
  // handlers before deallocation. Passing nil as completion handler (when
  // appropriate) may speed up web view deallocation, because there will be no
  // need to call those completion handlers.
  if (completion_handler) {
    web_view_completion_handler = ^(id result, NSError* error) {
      completion_handler(UIResultFromWKResult(result), error);
    };
  }
  [web_view evaluateJavaScript:script
             completionHandler:web_view_completion_handler];
}

}  // namespace web
