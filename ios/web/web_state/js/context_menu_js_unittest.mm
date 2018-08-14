// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/macros.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for ios/web/web_state/js/resources/context_menu.js.

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// A class which handles receiving script message responses by implementing the
// WKScriptMessageHandler protocol.
@interface CRWFakeScriptMessageHandler : NSObject<WKScriptMessageHandler>
@property(nonatomic) WKScriptMessage* lastReceivedScriptMessage;
@end

@implementation CRWFakeScriptMessageHandler
@synthesize lastReceivedScriptMessage = _lastReceivedScriptMessage;

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedScriptMessage = message;
}
@end

namespace {

// Request id used for __gCrWeb.findElementAtPoint call.
NSString* const kRequestId = @"UNIQUE_IDENTIFIER";

const char kTestUrl[] = "https://chromium.test/";

// Page html template for image tester helper functions.
NSString* kPageContentTemplate =
    @"<html><body style='margin-left:10px;margin-top:10px;'>"
     "<div style='width:100px;height:100px;'>"
     "  <p style='position:absolute;left:25px;top:25px;"
     "      width:50px;height:50px'>"
     "%@"
     "    Chrome rocks!"
     "  </p></div></body></html>";

}  // namespace

namespace web {

// Test fixture to test __gCrWeb.findElementAtPoint function defined in
// context_menu.js.
class ContextMenuJsFindElementAtPointTest : public web::WebTest {
 public:
  ContextMenuJsFindElementAtPointTest()
      : script_message_handler_([[CRWFakeScriptMessageHandler alloc] init]),
        web_view_(web::BuildWKWebView(CGRectMake(0.0, 0.0, 100.0, 100.0),
                                      GetBrowserState())) {
    [web_view_.configuration.userContentController
        addScriptMessageHandler:script_message_handler_
                           name:@"FindElementResultHandler"];
  }

 protected:
  // Returns details of the DOM element at the given |x| and |y| coordinates.
  // The given point is in the device's coordinate space.
  id FindElementAtPoint(CGFloat x, CGFloat y) {
    EXPECT_TRUE(web::WaitForInjectedScripts(web_view_));

    // Force layout
    web::ExecuteJavaScript(web_view_, @"document.getElementsByTagName('p')");

    // Clear previous script message response.
    script_message_handler_.lastReceivedScriptMessage = nil;

    ExecuteFindElementFromPointJavaScript(x, y);

    // Wait for response.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return !!script_message_handler_.lastReceivedScriptMessage;
    }));

    return script_message_handler_.lastReceivedScriptMessage.body;
  }

  // Returns web view's content size from the current web state.
  CGSize GetWebViewContentSize() { return web_view_.scrollView.contentSize; }

  // Returns the test page URL.
  NSURL* GetTestURL() { return net::NSURLWithGURL(GURL(kTestUrl)); }

  // Executes __gCrWeb.findElementAtPoint script. |x| and |y| are points in web
  // view coordinate system.
  id ExecuteFindElementFromPointJavaScript(CGFloat x, CGFloat y) {
    NSString* const script = [NSString
        stringWithFormat:@"__gCrWeb.findElementAtPoint('%@', %g, %g, %g, %g)",
                         kRequestId, x, y, GetWebViewContentSize().width,
                         GetWebViewContentSize().height];
    return web::ExecuteJavaScript(web_view_, script);
  }

  // Handles script message responses sent from |web_view_|.
  CRWFakeScriptMessageHandler* script_message_handler_;

  // The web view used for testing.
  WKWebView* web_view_;
};

#pragma mark - Image without link

// Tests that the correct src and referrer are found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementAtPoint) {
  NSString* image =
      @"<img id='foo' style='width:200;height:200;' src='file:///bogus'/>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that the correct title is found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementWithTitleAtPoint) {
  NSString* image =
      @"<img id='foo' title='Hello world!'"
       "style='width:200;height:200;' src='file:///bogus'/>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementTitle : @"Hello world!",
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that image details are not reutrned for a point outside of the document
// margins.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointOutsideDocument) {
  NSString* image =
      @"<img id='foo' style='width:200;height:200;' src='file:///bogus'/>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(0, 0);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that image details are not reutrned for a point outside of the element.
// TODO(crbug.com/796418): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FindImageElementAtPointOutsideElement \
  FindImageElementAtPointOutsideElement
#else
#define MAYBE_FindImageElementAtPointOutsideElement \
  FLAKY_FindImageElementAtPointOutsideElement
#endif

TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_FindImageElementAtPointOutsideElement) {
  NSString* image =
      @"<img id='foo' style='width:200;height:200;' src='file:///bogus'/>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(GetWebViewContentSize().width / 2, 50);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

#pragma mark - Image with link

// Tests that an image link returns details for both the image and the link
// destination when the image source is a file:// url.
// TODO(crbug.com/796418): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FindLinkImageAtPointForFileUrl FindLinkImageAtPointForFileUrl
#else
#define MAYBE_FindLinkImageAtPointForFileUrl \
  FLAKY_FindLinkImageAtPointForFileUrl
#endif
TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_FindLinkImageAtPointForFileUrl) {
  NSString* link_image =
      @"<a href='file:///linky'>"
       "<img id='foo' style='width:200;height:200;' src='file:///bogus'/>"
       "</a>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, link_image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"file:///linky",
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link does not return image and link details for a point
// outside the document.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointOutsideDocument) {
  NSString* link_image =
      @"<a href='file:///linky'>"
       "<img id='foo' style='width:200;height:200;' src='file:///bogus'/>"
       "</a>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, link_image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(0, 0);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link does not return image and link details for a point
// outside the element.
// TODO(crbug.com/796418): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FindLinkImageAtPointOutsideElement \
  FindLinkImageAtPointOutsideElement
#else
#define MAYBE_FindLinkImageAtPointOutsideElement \
  FLAKY_FindLinkImageAtPointOutsideElement
#endif
TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_FindLinkImageAtPointOutsideElement) {
  NSString* link_image =
      @"<a href='file:///linky'>"
       "<img id='foo' style='width:200;height:200;' src='file:///bogus'/>"
       "</a>";

  NSString* html = [NSString stringWithFormat:kPageContentTemplate, link_image];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(GetWebViewContentSize().width / 2, 50);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link returns details for both the image and the link
// destination when the image source is a relative url.
// TODO(crbug.com/817385): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FindLinkImageAtPointForRelativeUrl \
  FindLinkImageAtPointForRelativeUrl
#else
#define MAYBE_FindLinkImageAtPointForRelativeUrl \
  FLAKY_FindLinkImageAtPointForRelativeUrl
#endif
TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_FindLinkImageAtPointForRelativeUrl) {
  NSString* kLinkDest = @"http://destination/";
  NSString* kImageHtml =
      @"<a href='%@'><img width=400 height=400 src='foo'></img></a>";

  NSString* html = [NSString stringWithFormat:kImageHtml, kLinkDest];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : [NSString stringWithFormat:@"%sfoo", kTestUrl],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : kLinkDest,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for both the image and the link when
// the link points to JavaScript that is not a NOP.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageLinkedToJavaScript) {
  NSString* kImageHtml =
      @"<a href='javascript:console.log('whatever')'>"
       "<img width=400 height=400 src='foo'></img></a>";

  // A page with a link with some JavaScript that does not result in a NOP.
  ASSERT_TRUE(web::LoadHtml(web_view_, kImageHtml, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : [NSString stringWithFormat:@"%sfoo", kTestUrl],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"javascript:console.log(",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptSemicolon) {
  NSString* kImageHtml =
      @"<a href='javascript:;'><img width=400 height=400 src='foo'></img></a>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kImageHtml, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : [NSString stringWithFormat:@"%sfoo", kTestUrl],
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptVoid) {
  NSString* kImageHtml =
      @"<a href='javascript:void(0);'>"
       "<img width=400 height=400 src='foo'></img></a>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kImageHtml, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : [NSString stringWithFormat:@"%sfoo", kTestUrl],
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptMultipleVoid) {
  NSString* kImageHtml =
      @"<a href='javascript:void(0);  void(0); void(0)'>"
       "<img width=400 height=400 src='foo'></img></a>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kImageHtml, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : [NSString stringWithFormat:@"%sfoo", kTestUrl],
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that only the parent link details are returned for an image with
// "-webkit-touch-callout:none" style and a parent link.
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfImageWithCalloutNone) {
  NSString* kLinkDest = @"http://destination/";
  NSString* kImageHtml =
      @"<a href='%@'><img "
       "style='width:9;height:9;display:block;-webkit-touch-callout:none;'>"
       "</a>";

  NSString* html = [NSString stringWithFormat:kImageHtml, kLinkDest];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(5, 5);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : kLinkDest,
  };
  EXPECT_NSEQ(expected_result, result);
}

#pragma mark -

// Tests that a text input field prevents returning details for an image behind
// the field.
TEST_F(ContextMenuJsFindElementAtPointTest, TextAreaStopsProximity) {
  NSString* kHtml =
      @"<html><body style='margin-left:10px;margin-top:10px;'>"
       "<div style='width:100px;height:100px;'>"
       "<img id='foo'"
       "    style='position:absolute;left:0px;top:0px;width:50px;height:50px'"
       "    src='file:///bogus' />"
       "<input type='text' name='name'"
       "       style='position:absolute;left:5px;top:5px; "
       "width:40px;height:40px'/>"
       "</div></body> </html>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kHtml, GetTestURL()));

  id result = FindElementAtPoint(10, 10);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that __gCrWeb.findElementAtPoint reports "never" as the referrer
// policy for pages that have an unsupported policy in a meta tag.
TEST_F(ContextMenuJsFindElementAtPointTest, UnsupportedReferrerPolicy) {
  // A page with an unsupported referrer meta tag and a 400x400 image.
  NSString* kInvalidReferrerTag =
      @"<meta name=\"referrer\" content=\"unsupported-value\">"
       "<img width=400 height=400 src='foo'></img>";

  // Load the invalid meta tag
  ASSERT_TRUE(web::LoadHtml(web_view_, kInvalidReferrerTag, GetTestURL()));

  id result = FindElementAtPoint(20, 20);
  ASSERT_TRUE([result isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"never", result[kContextMenuElementReferrerPolicy]);
}

// Tests that __gCrWeb.findElementAtPoint finds an element at the bottom of a
// very long page.
// TODO(crbug.com/796418): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_LinkOfTextFromTallPage LinkOfTextFromTallPage
#else
#define MAYBE_LinkOfTextFromTallPage FLAKY_LinkOfTextFromTallPage
#endif
TEST_F(ContextMenuJsFindElementAtPointTest, MAYBE_LinkOfTextFromTallPage) {
  NSString* kHtml =
      @"<html><body>"
       " <div style='height:4000px'></div>"
       " <div><a href='http://destination'>link</a></div>"
       "</body></html>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kHtml, GetTestURL()));

  // Force layout to ensure |content_height| below is correct.
  web::ExecuteJavaScript(web_view_, @"document.getElementsByTagName('p')");

  // Scroll the webView to the bottom to make the link accessible.
  CGFloat content_height = GetWebViewContentSize().height;
  CGFloat scroll_view_height = CGRectGetHeight(web_view_.scrollView.frame);
  CGFloat offset = content_height - scroll_view_height;
  web_view_.scrollView.contentOffset = CGPointMake(0.0, offset);

  // Link is at bottom of the page content.
  id result = FindElementAtPoint(1, content_height - 5.0);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
// TODO(crbug.com/796418): This test is flaky on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_LinkOfTextWithoutCalloutProperty LinkOfTextWithoutCalloutProperty
#else
#define MAYBE_LinkOfTextWithoutCalloutProperty \
  FLAKY_LinkOfTextWithoutCalloutProperty
#endif
TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_LinkOfTextWithoutCalloutProperty) {
  NSString* kLinkDest = @"http://destination/";
  NSString* kLinkHtml = @"<a href='%@'>link</a>";

  NSString* html = [NSString stringWithFormat:kLinkHtml, kLinkDest];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : kLinkDest,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is set to default. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
// TODO(crbug.com/796343): This test is flaky on iOS 11 device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_LinkOfTextWithCalloutDefault LinkOfTextWithCalloutDefault
#else
#define MAYBE_LinkOfTextWithCalloutDefault FLAKY_LinkOfTextWithCalloutDefault
#endif
TEST_F(ContextMenuJsFindElementAtPointTest,
       MAYBE_LinkOfTextWithCalloutDefault) {
  NSString* kLinkDest = @"http://destination/";
  NSString* kLinkHtml =
      @"<a href='%@' style='-webkit-touch-callout:default;'>"
       "link</a>";

  NSString* html = [NSString stringWithFormat:kLinkHtml, kLinkDest];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : kLinkDest,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that no callout information about a link is displayed when
// -webkit-touch-callout property is set to none. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutNone) {
  NSString* kLinkHtml =
      @"<a href='http://destination' "
       "style='-webkit-touch-callout:none;'>link</a>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kLinkHtml, GetTestURL()));

  id result = FindElementAtPoint(1, 1);
  EXPECT_NSEQ(@{kContextMenuElementRequestId : kRequestId}, result);
}

// Tests that -webkit-touch-callout property can be inherited from ancester
// if it's not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutFromAncester) {
  NSString* kLinkHtml =
      @"<body style='-webkit-touch-callout: none'>"
       " <a href='http://destination'>link</a>"
       "</body>";

  ASSERT_TRUE(web::LoadHtml(web_view_, kLinkHtml, GetTestURL()));

  id result = FindElementAtPoint(1, 1);
  EXPECT_NSEQ(@{kContextMenuElementRequestId : kRequestId}, result);
}

// Tests that setting -webkit-touch-callout property can override the value
// inherited from ancester. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutOverride) {
  NSString* kLinkDest = @"http://destination/";
  NSString* kLinkHtml =
      @"<body style='-webkit-touch-callout: none'>"
       " <a href='%@' style='-webkit-touch-callout: default'>link</a>"
       "</body>";

  NSString* html = [NSString stringWithFormat:kLinkHtml, kLinkDest];
  ASSERT_TRUE(web::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : kLinkDest,
  };
  EXPECT_NSEQ(expected_result, result);
}

}  // namespace web
