// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#import <WebKit/WebKit.h>

#include "base/mac/bind_objc_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/testing/wait_util.h"
#import "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"

using testing::WaitUntilConditionOrTimeout;

namespace {

// Script that returns document.body as a string.
char kGetDocumentBodyJavaScript[] =
    "document.body ? document.body.textContent : null";
// Script that tests presence of css selector.
char kTestCssSelectorJavaScriptTemplate[] = "!!document.querySelector(\"%s\");";

// Helper function for matching web views containing or not containing |text|,
// depending on the value of |should_contain_text|.
id<GREYMatcher> webViewWithText(std::string text,
                                web::WebState* web_state,
                                bool should_contain_text) {
  MatchesBlock matches = ^BOOL(WKWebView*) {
    return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      std::unique_ptr<base::Value> value =
          web::test::ExecuteJavaScript(web_state, kGetDocumentBodyJavaScript);
      std::string body;
      if (value && value->GetAsString(&body)) {
        BOOL contains_text = body.find(text) != std::string::npos;
        return contains_text == should_contain_text;
      }
      return false;
    });
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:should_contain_text ? @"web view containing "
                                                : @"web view not containing "];
    [description appendText:base::SysUTF8ToNSString(text)];
  };

  return grey_allOf(webViewInWebState(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

}  // namespace

namespace web {

id<GREYMatcher> webViewInWebState(WebState* web_state) {
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

id<GREYMatcher> webViewContainingText(std::string text, WebState* web_state) {
  return webViewWithText(text, web_state, true);
}

id<GREYMatcher> webViewNotContainingText(std::string text,
                                         WebState* web_state) {
  return webViewWithText(text, web_state, false);
}

id<GREYMatcher> webViewContainingBlockedImage(std::string image_id,
                                              CGSize expected_size,
                                              WebState* web_state) {
  MatchesBlock matches = ^BOOL(WKWebView*) {
    return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      NSString* const kGetElementAttributesScript = [NSString
          stringWithFormat:@"var image = document.getElementById('%@');"
                           @"var imageHeight = image.height;"
                           @"var imageWidth = image.width;"
                           @"JSON.stringify({"
                           @"  height:imageHeight,"
                           @"  width:imageWidth"
                           @"});",
                           base::SysUTF8ToNSString(image_id)];
      std::unique_ptr<base::Value> value = web::test::ExecuteJavaScript(
          web_state, base::SysNSStringToUTF8(kGetElementAttributesScript));
      std::string result;
      if (value && value->GetAsString(&result)) {
        NSString* evaluation_result = base::SysUTF8ToNSString(result);
        NSData* image_attributes_as_data =
            [evaluation_result dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary* image_attributes =
            [NSJSONSerialization JSONObjectWithData:image_attributes_as_data
                                            options:0
                                              error:nil];
        CGFloat height = [image_attributes[@"height"] floatValue];
        CGFloat width = [image_attributes[@"width"] floatValue];
        return (height < expected_size.height && width < expected_size.width);
      }
      return false;
    });
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view blocking resource with id "];
    [description appendText:base::SysUTF8ToNSString(image_id)];
  };

  return grey_allOf(webViewInWebState(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

id<GREYMatcher> webViewCssSelector(std::string selector, WebState* web_state) {
  MatchesBlock matches = ^BOOL(WKWebView*) {
    std::string script = base::StringPrintf(kTestCssSelectorJavaScriptTemplate,
                                            selector.c_str());
    return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      bool did_succeed = false;
      std::unique_ptr<base::Value> value =
          web::test::ExecuteJavaScript(web_state, script);
      if (value) {
        value->GetAsBoolean(&did_succeed);
      }
      return did_succeed;
    });
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view selector "];
    [description appendText:base::SysUTF8ToNSString(selector)];
  };

  return grey_allOf(webViewInWebState(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

id<GREYMatcher> webViewScrollView(WebState* web_state) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[UIScrollView class]] &&
           [view.superview isKindOfClass:[WKWebView class]] &&
           [view isDescendantOfView:web_state->GetView()];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view scroll view"];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

id<GREYMatcher> interstitialContainingText(NSString* text,
                                           WebState* web_state) {
  MatchesBlock matches = ^BOOL(WKWebView* view) {
    return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      web::WebInterstitialImpl* interstitial =
          static_cast<web::WebInterstitialImpl*>(
              web_state->GetWebInterstitial());
      if (![view isDescendantOfView:interstitial->GetContentView()])
        return false;

      NSString* script = base::SysUTF8ToNSString(kGetDocumentBodyJavaScript);
      id body = ExecuteScriptOnInterstitial(web_state, script);
      return [body containsString:text] ? true : false;
    });
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"interstitial containing "];
    [description appendText:text];
  };

  return grey_allOf(webViewInWebState(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

}  // namespace web
