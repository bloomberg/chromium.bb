// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

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
    @"src" : @"file:///bogus",
    @"referrerPolicy" : @"default",
  };
  ImageTesterHelper(html, expected_value);
}

// Tests that __gCrWeb.getElementFromPoint function returns correct title.
TEST_F(ContextMenuJsTest, GetImageTitleAtPoint) {
  NSString* html = @"<img id='foo' title='Hello world!'"
                    "style='width:200;height:200;' src='file:///bogus'/>";
  NSDictionary* expected_value = @{
    @"src" : @"file:///bogus",
    @"referrerPolicy" : @"default",
    @"title" : @"Hello world!",
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
    @"src" : @"file:///bogus",
    @"referrerPolicy" : @"default",
    @"href" : @"file:///linky",
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
    @"src" : @"file:///bogus",
    @"referrerPolicy" : @"default",
  };
  NSDictionary* failure = @{};

  TestCoordinatesAndExpectedValue test_data[] = {
      {2, 20, success}, {10, 10, failure},
  };

  for (size_t i = 0; i < arraysize(test_data); i++) {
    const TestCoordinatesAndExpectedValue& data = test_data[i];
    LoadHtml(html);
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
  id result = ExecuteGetElementFromPointJavaScript(20, 20);
  NSDictionary* expected_result = @{
    @"src" : [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
    @"referrerPolicy" : @"default",
    @"href" : @"http://destination/",
  };
  EXPECT_NSEQ(expected_result, result);

  // A page with a link with some JavaScript that does not result in a NOP.
  LoadHtml(base::StringPrintf(image, "javascript:console.log('whatever')"));
  result = ExecuteGetElementFromPointJavaScript(20, 20);
  expected_result = @{
    @"src" : [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
    @"referrerPolicy" : @"default",
    @"href" : @"javascript:console.log(",
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
      @"src" : [NSString stringWithFormat:@"%sfoo", BaseUrl().c_str()],
      @"referrerPolicy" : @"default",
    };
    // Make sure the returned JSON does not have an 'href' key.
    EXPECT_NSEQ(expected_result, result);
  }
}

}  // namespace web
