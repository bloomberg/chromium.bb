// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/web_state/context_menu_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for ios/web/web_state/js/resources/context_menu.js.

namespace {

// Test coordinates and expected result for __gCrWeb.getElementFromPoint call.
struct TestCoordinatesAndExpectedValue {
  TestCoordinatesAndExpectedValue(CGFloat x, CGFloat y, id expected_value)
      : x(x), y(y), expected_value(expected_value) {}
  CGFloat x = 0;
  CGFloat y = 0;
  id expected_value = nil;
};

}  // namespace

namespace web {

// Test fixture to test context_menu.js.
class ContextMenuJsTest : public web::WebTestWithWebState {
 protected:
  // Verifies that __gCrWeb.getElementFromPoint returns |expected_value| for
  // the given image |html|.
  void ImageTesterHelper(NSString* html, NSDictionary* expected_value) {
    NSString* page_content_template =
        @"<html><body style='margin-left:10px;margin-top:10px;'>"
         "<div style='width:100px;height:100px;'>"
         "  <p style='position:absolute;left:25px;top:25px;"
         "      width:50px;height:50px'>"
         "%@"
         "    Chrome rocks!"
         "  </p></div></body></html>";
    NSString* page_content =
        [NSString stringWithFormat:page_content_template, html];

    TestCoordinatesAndExpectedValue test_data[] = {
        // Point outside the document margins.
        {0, 0, @{}},
        // Point inside the <p> element.
        {20, 20, expected_value},
        // Point outside the <p> element.
        {GetWebViewContentSize().width / 2, 50, @{}},
    };
    for (size_t i = 0; i < arraysize(test_data); i++) {
      const TestCoordinatesAndExpectedValue& data = test_data[i];
      LoadHtml(page_content);
      ExecuteJavaScript(@"document.getElementsByTagName('p')");  // Force layout
      id result = ExecuteGetElementFromPointJavaScript(data.x, data.y);
      EXPECT_NSEQ(data.expected_value, result)
          << " in test " << i << ": (" << data.x << ", " << data.y << ")";
    }
  }
  // Returns web view's content size from the current web state.
  CGSize GetWebViewContentSize() {
    return web_state()->GetWebViewProxy().scrollViewProxy.contentSize;
  }

  // Executes __gCrWeb.getElementFromPoint script and syncronously returns the
  // result. |x| and |y| are points in web view coordinate system.
  id ExecuteGetElementFromPointJavaScript(CGFloat x, CGFloat y) {
    CGSize contentSize = GetWebViewContentSize();
    NSString* const script = [NSString
        stringWithFormat:@"__gCrWeb.getElementFromPoint(%g, %g, %g, %g)", x, y,
                         contentSize.width, contentSize.height];
    return ExecuteJavaScript(script);
  }
};

// Tests that __gCrWeb.getElementFromPoint function returns correct src.
TEST_F(ContextMenuJsTest, GetImageUrlAtPoint) {
  NSString* html =
      @"<img id='foo' style='width:200;height:200;' src='file:///bogus'/>";
  NSDictionary* expected_value = @{
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
  };
  ImageTesterHelper(html, expected_value);
}

// Tests that __gCrWeb.getElementFromPoint function returns correct title.
TEST_F(ContextMenuJsTest, GetImageTitleAtPoint) {
  NSString* html = @"<img id='foo' title='Hello world!'"
                    "style='width:200;height:200;' src='file:///bogus'/>";
  NSDictionary* expected_value = @{
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementTitle : @"Hello world!",
  };
  ImageTesterHelper(html, expected_value);
}

// Tests that __gCrWeb.getElementFromPoint function returns correct href.
TEST_F(ContextMenuJsTest, GetLinkImageUrlAtPoint) {
  NSString* html =
      @"<a href='file:///linky'>"
       "<img id='foo' style='width:200;height:200;' src='file:///bogus'/>"
       "</a>";
  NSDictionary* expected_value = @{
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"file:///linky",
  };
  ImageTesterHelper(html, expected_value);
}

TEST_F(ContextMenuJsTest, TextAreaStopsProximity) {
  NSString* html =
      @"<html><body style='margin-left:10px;margin-top:10px;'>"
       "<div style='width:100px;height:100px;'>"
       "<img id='foo'"
       "    style='position:absolute;left:0px;top:0px;width:50px;height:50px'"
       "    src='file:///bogus' />"
       "<input type='text' name='name'"
       "       style='position:absolute;left:5px;top:5px; "
       "width:40px;height:40px'/>"
       "</div></body> </html>";

  NSDictionary* success = @{
    kContextMenuElementSource : @"file:///bogus",
    kContextMenuElementReferrerPolicy : @"default",
  };
  NSDictionary* failure = @{};

  TestCoordinatesAndExpectedValue test_data[] = {
      {2, 20, success}, {10, 10, failure},
  };

  for (size_t i = 0; i < arraysize(test_data); i++) {
    const TestCoordinatesAndExpectedValue& data = test_data[i];
    LoadHtml(html);
    ExecuteJavaScript(@"document.getElementsByTagName('img')");  // Force layout
    id result = ExecuteGetElementFromPointJavaScript(data.x, data.y);
    EXPECT_NSEQ(data.expected_value, result)
        << " in test " << i << ": (" << data.x << ", " << data.y << ")";
  }
}

// Tests the javascript of the url of the an image present in the DOM.
TEST_F(ContextMenuJsTest, LinkOfImage) {
  // A page with a large image surrounded by a link.
  static const char image[] =
      "<a href='%s'><img width=400 height=400 src='foo'></img></a>";

  // A page with a link to a destination URL.
  LoadHtml(base::StringPrintf(image, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('img')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(20, 20);
  NSDictionary* expected_result = @{
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);

  // A page with a link with some JavaScript that does not result in a NOP.
  LoadHtml(base::StringPrintf(image, "javascript:console.log('whatever')"));
  result = ExecuteGetElementFromPointJavaScript(20, 20);
  expected_result = @{
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"javascript:console.log(",
  };
  EXPECT_NSEQ(expected_result, result);

  // A list of JavaScripts that result in a NOP.
  std::vector<std::string> nop_javascripts;
  nop_javascripts.push_back(";");
  nop_javascripts.push_back("void(0);");
  nop_javascripts.push_back("void(0);  void(0); void(0)");

  for (auto js : nop_javascripts) {
    // A page with a link with some JavaScript that results in a NOP.
    const std::string javascript = std::string("javascript:") + js;
    LoadHtml(base::StringPrintf(image, javascript.c_str()));
    result = ExecuteGetElementFromPointJavaScript(20, 20);
    expected_result = @{
      kContextMenuElementSource :
          [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
      kContextMenuElementReferrerPolicy : @"default",
    };
    // Make sure the returned JSON does not have an 'href' key.
    EXPECT_NSEQ(expected_result, result);
  }
}

// Tests context menu invoked on an image with "-webkit-touch-callout:none"
// style and parent link.
TEST_F(ContextMenuJsTest, LinkOfImageWithCalloutNone) {
  // A page with an image surrounded by a link.
  static const char image_html[] =
      "<a href='%s'>"
      "<img style='width:9;height:9;display:block;-webkit-touch-callout:none;'>"
      "</a>";

  // A page with a link to a destination URL.
  LoadHtml(base::StringPrintf(image_html, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('img')");  // Force layout.
  id result = ExecuteGetElementFromPointJavaScript(5, 5);
  NSDictionary* expected_result = @{
    kContextMenuElementInnerText : @"",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that the GetElementFromPoint script reports "never" as the referrer
// policy for pages that have an unsupported policy in a meta tag.
TEST_F(ContextMenuJsTest, UnsupportedReferrerPolicy) {
  // A page with an unsupported referrer meta tag and a 400x400 image.
  static const char kInvalidReferrerTag[] =
      "<meta name=\"referrer\" content=\"unsupported-value\">"
      "<img width=400 height=400 src='foo'></img>";

  // Load the invalid meta tag
  LoadHtml(kInvalidReferrerTag);
  ExecuteJavaScript(@"document.getElementsByTagName('img')");  // Force layout
  id result = ExecuteGetElementFromPointJavaScript(20, 20);
  ASSERT_TRUE([result isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"never", result[kContextMenuElementReferrerPolicy]);
}

// Tests that getElementFromPoint finds an element at the bottom of a very long
// page.
TEST_F(ContextMenuJsTest, FLAKY_LinkOfTextFromTallPage) {
  const char kHtml[] =
      "<html><body>"
      " <div style='height:4000px'></div>"
      " <div><a href='http://destination'>link</a></div>"
      "</body></html>";
  LoadHtml(kHtml);

  // Scroll the webView to the bottom to make the link accessible.
  CGFloat content_height = GetWebViewContentSize().height;
  CGFloat scroll_view_height =
      CGRectGetHeight(web_state()->GetWebViewProxy().scrollViewProxy.frame);
  CGFloat offset = content_height - scroll_view_height;
  web_state()->GetWebViewProxy().scrollViewProxy.contentOffset =
      CGPointMake(0.0, offset);

  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  // Link is at bottom of the page content.
  id result = ExecuteGetElementFromPointJavaScript(1, content_height - 5.0);
  NSDictionary* expected_result = @{
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsTest, LinkOfTextWithoutCalloutProperty) {
  const char kLinkHtml[] = "<a href='%s'>link</a>";

  LoadHtml(base::StringPrintf(kLinkHtml, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is set to default. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsTest, LinkOfTextWithCalloutDefault) {
  const char kLinkHtml[] =
      "<a href='%s' style='-webkit-touch-callout:default;'>link</a>";

  LoadHtml(base::StringPrintf(kLinkHtml, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that no callout information about a link is displayed when
// -webkit-touch-callout property is set to none. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsTest, LinkOfTextWithCalloutNone) {
  const char kLinkHtml[] =
      "<a href='%s' style='-webkit-touch-callout:none;'>link</a>";

  LoadHtml(base::StringPrintf(kLinkHtml, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(1, 1);
  EXPECT_NSEQ(@{}, result);
}

// Tests that -webkit-touch-callout property can be inherited from ancester if
// it's not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsTest, LinkOfTextWithCalloutFromAncester) {
  const char kLinkHtml[] =
      "<body style='-webkit-touch-callout: none'>"
      " <a href='%s'>link</a>"
      "</body>";

  LoadHtml(base::StringPrintf(kLinkHtml, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(1, 1);
  EXPECT_NSEQ(@{}, result);
}

// Tests that setting -webkit-touch-callout property can override the value
// inherited from ancester. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsTest, LinkOfTextWithCalloutOverride) {
  const char kLinkHtml[] =
      "<body style='-webkit-touch-callout: none'>"
      " <a href='%s' style='-webkit-touch-callout: default'>link</a>"
      "</body>";

  LoadHtml(base::StringPrintf(kLinkHtml, "http://destination"));
  ExecuteJavaScript(@"document.getElementsByTagName('a')");  // Force layout.

  id result = ExecuteGetElementFromPointJavaScript(1, 1);
  NSDictionary* expected_result = @{
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);
}

}  // namespace web
