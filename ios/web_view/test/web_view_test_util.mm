// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/web_view_test_util.h"

#import "ios/web_view/public/cwv_web_view.h"
#import "ios/web_view/public/cwv_web_view_configuration.h"

#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;

namespace ios_web_view {
namespace test {

CWVWebView* CreateWebView() {
  return [[CWVWebView alloc]
      initWithFrame:UIScreen.mainScreen.bounds
      configuration:[CWVWebViewConfiguration defaultConfiguration]];
}

bool LoadUrl(CWVWebView* web_view, NSURL* url) {
  [web_view loadRequest:[NSURLRequest requestWithURL:url]];

  return WaitForWebViewLoadCompletionOrTimeout(web_view);
}

bool TapWebViewElementWithId(CWVWebView* web_view, NSString* element_id) {
  // TODO(crbug.com/725524): Share this script with Chrome.
  NSString* script = [NSString
      stringWithFormat:@"(function() {"
                        "  var element = document.getElementById('%@');"
                        "  if (element) {"
                        "    element.click();"
                        "    return true;"
                        "  }"
                        "  return false;"
                        "})();",
                       element_id];
  return [EvaluateJavaScript(web_view, script, nil) boolValue];
}

id EvaluateJavaScript(CWVWebView* web_view, NSString* script, NSError** error) {
  __block bool did_complete = false;
  __block id evaluation_result = nil;
  __block id evaluation_error = nil;
  [web_view evaluateJavaScript:script
             completionHandler:^(id local_result, NSError* local_error) {
               did_complete = true;
               evaluation_result = [local_result copy];
               evaluation_error = [local_error copy];
             }];

  WaitUntilConditionOrTimeout(testing::kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  if (error)
    *error = evaluation_error;

  return evaluation_result;
}

bool WaitForWebViewContainingTextOrTimeout(CWVWebView* web_view,
                                           NSString* text) {
  return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
    id body = ios_web_view::test::EvaluateJavaScript(
        web_view, @"document.body ? document.body.textContent : null", nil);
    return [body isKindOfClass:[NSString class]] && [body containsString:text];
  });
}

bool WaitForWebViewLoadCompletionOrTimeout(CWVWebView* web_view) {
  return WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_view.loading;
  });
}

}  // namespace test
}  // namespace ios_web_view
