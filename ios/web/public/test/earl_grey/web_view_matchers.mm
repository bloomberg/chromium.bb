// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/mac/bind_objc_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "ios/testing/wait_util.h"
#import "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "net/base/mac/url_conversions.h"

using testing::kWaitForDownloadTimeout;
using testing::WaitUntilConditionOrTimeout;

// A helper delegate class that allows downloading responses with invalid
// SSL certs.
@interface TestURLSessionDelegate : NSObject<NSURLSessionDelegate>
@end

@implementation TestURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
  completionHandler(NSURLSessionAuthChallengeUseCredential,
                    [NSURLCredential credentialForTrust:serverTrust]);
}

@end

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

// Fetches the image from |image_url|.
UIImage* LoadImage(const GURL& image_url) {
  __block base::scoped_nsobject<UIImage> image;
  __block base::scoped_nsobject<NSError> error;
  TestURLSessionDelegate* session_delegate =
      [[TestURLSessionDelegate alloc] init];
  NSURLSessionConfiguration* session_config =
      [NSURLSessionConfiguration defaultSessionConfiguration];
  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:session_config
                                    delegate:session_delegate
                               delegateQueue:nil];
  id completion_handler = ^(NSData* data, NSURLResponse*, NSError* task_error) {
    if (task_error) {
      error.reset([task_error retain]);
    } else {
      image.reset([[UIImage alloc] initWithData:data]);
    }
    [session_delegate autorelease];
  };

  NSURLSessionDataTask* task =
      [session dataTaskWithURL:net::NSURLWithGURL(image_url)
             completionHandler:completion_handler];
  [task resume];

  bool image_loaded = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return image || error;
  });
  GREYAssert(image_loaded, @"Failed to download image");

  return [[image retain] autorelease];
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
                                              WebState* web_state) {
  std::string get_url_script =
      base::StringPrintf("document.getElementById('%s').src", image_id.c_str());
  std::unique_ptr<base::Value> url_as_value =
      test::ExecuteJavaScript(web_state, get_url_script);
  std::string url_as_string;
  if (!url_as_value->GetAsString(&url_as_string))
    return grey_nil();

  UIImage* image = LoadImage(GURL(url_as_string));
  if (!image)
    return grey_nil();

  return webViewContainingBlockedImage(image_id, image.size, web_state);
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

id<GREYMatcher> interstitial(WebState* web_state) {
  MatchesBlock matches = ^BOOL(WKWebView* view) {
    web::WebInterstitialImpl* interstitial =
        static_cast<web::WebInterstitialImpl*>(web_state->GetWebInterstitial());
    return interstitial &&
           [view isDescendantOfView:interstitial->GetContentView()];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"interstitial displayed"];
  };

  return grey_allOf(webViewInWebState(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

id<GREYMatcher> interstitialContainingText(NSString* text,
                                           WebState* web_state) {
  MatchesBlock matches = ^BOOL(WKWebView* view) {
    return WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      NSString* script = base::SysUTF8ToNSString(kGetDocumentBodyJavaScript);
      id body = ExecuteScriptOnInterstitial(web_state, script);
      return [body containsString:text] ? true : false;
    });
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"interstitial containing "];
    [description appendText:text];
  };

  return grey_allOf(interstitial(web_state),
                    [[[GREYElementMatcherBlock alloc]
                        initWithMatchesBlock:matches
                            descriptionBlock:describe] autorelease],
                    nil);
}

}  // namespace web
