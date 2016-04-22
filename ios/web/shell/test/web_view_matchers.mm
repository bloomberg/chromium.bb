// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/web_view_matchers.h"

#import <WebKit/WebKit.h>

#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/shell/test/navigation_test_util.h"
#include "ios/web/shell/test/web_shell_test_util.h"

namespace {

// Constant for UI wait loop in seconds.
const NSTimeInterval kSpinDelaySeconds = 0.01;

// Constant for timeout in seconds while waiting for web element.
const NSTimeInterval kWaitForWebElementTimeout = 4.0;

// Script that returns document.body as a string.
char kGetDocumentBodyJavaScript[] =
    "document.body ? document.body.textContent : null";
}

namespace web {

id<GREYMatcher> webViewContainingText(NSString* text) {
  return [GREYMatchers matcherForWebViewContainingText:text];
}

}  // namespace web

@implementation GREYMatchers (WebViewAdditions)

+ (id<GREYMatcher>)matcherForWebViewContainingText:(NSString*)text {
  MatchesBlock matches = ^BOOL(UIView* view) {
    if (![view isKindOfClass:[WKWebView class]]) {
      return NO;
    }

    ViewController* viewController =
        web::web_shell_test_util::GetCurrentViewController();
    web::WebState* webState = [viewController webState];

    __block BOOL didSucceed = NO;
    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:kWaitForWebElementTimeout];
    while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
           !didSucceed) {
      webState->ExecuteJavaScript(
          base::UTF8ToUTF16(kGetDocumentBodyJavaScript),
          base::BindBlock(^(const base::Value* value) {
            std::string response;
            if (value && value->IsType(base::Value::TYPE_STRING) &&
                value->GetAsString(&response)) {
              didSucceed = response.find(base::SysNSStringToUTF8(text)) !=
                           std::string::npos;
            }
          }));
      base::test::ios::SpinRunLoopWithMaxDelay(
          base::TimeDelta::FromSecondsD(kSpinDelaySeconds));
    }
    return didSucceed;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"web view containing %@", text]];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

@end