// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#include <memory>

#import <WebKit/WebKit.h>

#include "base/mac/bind_objc_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/testing/earl_grey/wait_util.h"

namespace {

// Script that returns document.body as a string.
char kGetDocumentBodyJavaScript[] =
    "document.body ? document.body.textContent : null";
// Script that tests presence of css selector.
char kTestCssSelectorJavaScriptTemplate[] = "!!document.querySelector(\"%s\");";

// Matcher for WKWebView which belogs to the given |webState|.
id<GREYMatcher> webViewInWebState(web::WebState* web_state) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[WKWebView class]] &&
           [view isDescendantOfView:web_state->GetView()];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view in web state"];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

// Synchronously returns the result of executed JavaScript.
std::unique_ptr<base::Value> ExecuteScript(web::WebState* web_state,
                                           const std::string& script) {
  __block std::unique_ptr<base::Value> result;
  __block bool did_finish = false;
  web_state->ExecuteJavaScript(base::UTF8ToUTF16(script),
                               base::BindBlock(^(const base::Value* value) {
                                 if (value)
                                   result = value->CreateDeepCopy();
                                 did_finish = true;
                               }));

  testing::WaitUntilCondition(testing::kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });

  return result;
}

}  // namespace

namespace web {

id<GREYMatcher> webViewContainingText(const std::string& text,
                                      web::WebState* webState) {
  return
      [GREYMatchers matcherForWebViewContainingText:text inWebState:webState];
}

id<GREYMatcher> webViewCssSelector(const std::string& selector,
                                   web::WebState* webState) {
  return
      [GREYMatchers matcherForWebWithCSSSelector:selector inWebState:webState];
}

id<GREYMatcher> webViewScrollView(web::WebState* webState) {
  return [GREYMatchers matcherForWebViewScrollViewInWebState:webState];
}

}  // namespace web

@implementation GREYMatchers (WebViewAdditions)

+ (id<GREYMatcher>)matcherForWebViewContainingText:(const std::string&)text
                                        inWebState:(web::WebState*)webState {
  std::string textCopyForBlock = text;
  MatchesBlock matches = ^BOOL(WKWebView*) {
    __block BOOL didSucceed = NO;
    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:testing::kWaitForUIElementTimeout];
    while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
           !didSucceed) {
      std::unique_ptr<base::Value> value =
          ExecuteScript(webState, kGetDocumentBodyJavaScript);
      std::string body;
      if (value && value->GetAsString(&body)) {
        didSucceed = body.find(textCopyForBlock) != std::string::npos;
      }
      base::test::ios::SpinRunLoopWithMaxDelay(
          base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
    }
    return didSucceed;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view containing "];
    [description appendText:base::SysUTF8ToNSString(textCopyForBlock)];
  };

  return grey_allOf(webViewInWebState(webState),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

+ (id<GREYMatcher>)matcherForWebWithCSSSelector:(const std::string&)selector
                                     inWebState:(web::WebState*)webState {
  MatchesBlock matches = ^BOOL(WKWebView*) {
    std::string script = base::StringPrintf(kTestCssSelectorJavaScriptTemplate,
                                            selector.c_str());
    __block bool didSucceed = false;
    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:testing::kWaitForUIElementTimeout];
    while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
           !didSucceed) {
      std::unique_ptr<base::Value> value = ExecuteScript(webState, script);
      if (value)
        value->GetAsBoolean(&didSucceed);
      base::test::ios::SpinRunLoopWithMaxDelay(
          base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
    }
    return didSucceed;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view selector "];
    [description appendText:base::SysUTF8ToNSString(selector)];
  };

  return grey_allOf(webViewInWebState(webState),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
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
