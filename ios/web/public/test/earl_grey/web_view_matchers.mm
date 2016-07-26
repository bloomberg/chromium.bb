// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#import <WebKit/WebKit.h>

#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/testing/earl_grey/wait_util.h"

namespace {

// Script that returns document.body as a string.
char kGetDocumentBodyJavaScript[] =
    "document.body ? document.body.textContent : null";
}

namespace web {

id<GREYMatcher> webViewContainingText(const std::string& text,
                                      web::WebState* webState) {
  return
      [GREYMatchers matcherForWebViewContainingText:text inWebState:webState];
}

id<GREYMatcher> webViewScrollView(web::WebState* webState) {
  return [GREYMatchers matcherForWebViewScrollViewInWebState:webState];
}

}  // namespace web

@implementation GREYMatchers (WebViewAdditions)

+ (id<GREYMatcher>)matcherForWebViewContainingText:(const std::string&)text
                                        inWebState:(web::WebState*)webState {
  std::string textCopyForBlock = text;
  MatchesBlock matches = ^BOOL(UIView* view) {
    if (![view isKindOfClass:[WKWebView class]]) {
      return NO;
    }
    if (![view isDescendantOfView:webState->GetView()]) {
      return NO;
    }

    __block BOOL didSucceed = NO;
    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:testing::kWaitForUIElementTimeout];
    while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
           !didSucceed) {
      webState->ExecuteJavaScript(
          base::UTF8ToUTF16(kGetDocumentBodyJavaScript),
          base::BindBlock(^(const base::Value* value) {
            std::string response;
            if (value && value->IsType(base::Value::TYPE_STRING) &&
                value->GetAsString(&response)) {
              didSucceed = response.find(textCopyForBlock) != std::string::npos;
            }
          }));
      base::test::ios::SpinRunLoopWithMaxDelay(
          base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
    }
    return didSucceed;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view containing "];
    [description appendText:base::SysUTF8ToNSString(textCopyForBlock)];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

+ (id<GREYMatcher>)matcherForWebViewScrollViewInWebState:
    (web::WebState*)webState {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[UIScrollView class]] &&
           [view.superview isKindOfClass:[WKWebView class]] &&
           [view isDescendantOfView:webState->GetView()];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view scroll view"];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

@end
