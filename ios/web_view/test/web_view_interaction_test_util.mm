// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/web_view_interaction_test_util.h"

#import <ChromeWebView/ChromeWebView.h>

#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace test {

bool TapChromeWebViewElementWithId(CWVWebView* web_view, NSString* element_id) {
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
  __block bool did_complete = false;
  __block bool element_found = false;
  [web_view evaluateJavaScript:script
             completionHandler:^(id result, NSError*) {
               did_complete = true;
               element_found = [result boolValue];
             }];

  testing::WaitUntilConditionOrTimeout(testing::kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  return element_found;
}

}  // namespace test
}  // namespace ios_web_view
