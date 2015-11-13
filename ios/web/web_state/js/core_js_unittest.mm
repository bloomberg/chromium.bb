// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <vector>

#include "base/strings/stringprintf.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

// Unit tests for ios/web/web_state/js/resources/core.js.

namespace {

struct TestScriptAndExpectedValue {
  NSString* testScript;
  NSString* expectedValue;
};

// A mixin class for testing with CRWWKWebViewWebController or
// CRWUIWebViewWebController.
template <typename WebTestT>
class CoreJsTest : public WebTestT {
 protected:
  void ImageTesterHelper(
      NSString* htmlForImage,
      NSString* expectedValueWhenClickingOnImage) {
    NSString* pageContentTemplate =
        @"<html><body style='margin-left:10px;margin-top:10px;'>"
            "<div style='width:100px;height:100px;'>"
            "  <p style='position:absolute;left:25px;top:25px;"
            "      width:50px;height:50px'>"
            "%@"
            "    Chrome rocks!"
            "  </p></div></body></html>";
    NSString* pageContent =
        [NSString stringWithFormat:pageContentTemplate, htmlForImage];

    TestScriptAndExpectedValue testData[] = {
      // Point outside the document margins.
      {
        @"__gCrWeb.getElementFromPoint(0, 0)",
        @"{}"
      },
      // Point outside the <p> element.
      {
        @"__gCrWeb.getElementFromPoint(100, 100)",
        expectedValueWhenClickingOnImage
      },
      // Point inside the <p> element.
      {
        @"__gCrWeb.getElementFromPoint(300, 300)",
        @"{}"
      },
    };
    for (size_t i = 0; i < arraysize(testData); i++) {
      TestScriptAndExpectedValue& data = testData[i];
      WebTestT::LoadHtml(pageContent);
      NSString* result = WebTestT::RunJavaScript(data.testScript);
      EXPECT_NSEQ(data.expectedValue, result) << " in test " << i << ": " <<
          [data.testScript UTF8String];
    }
  }
};

// Concrete test fixture to test core.js using UIWebView-based web controller.
typedef CoreJsTest<web::WebTestWithUIWebViewWebController> CoreJSUIWebViewTest;

// Concrete test fixture to test core.js using WKWebView-based web controller.
typedef CoreJsTest<web::WebTestWithWKWebViewWebController> CoreJSWKWebViewTest;

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, GetImageUrlAtPoint) {
  NSString* htmlForImage =
      @"<img id='foo' style='width:200;height:200;' src='file:///bogus'/>";
  NSString* expectedValueWhenClickingOnImage =
      @"{\"src\":\"file:///bogus\",\"referrerPolicy\":\"default\"}";
  this->ImageTesterHelper(htmlForImage, expectedValueWhenClickingOnImage);
}

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, GetImageTitleAtPoint) {
  NSString* htmlForImage =
      @"<img id='foo' title='Hello world!'"
      "style='width:200;height:200;' src='file:///bogus'/>";
  NSString* expectedValueWhenClickingOnImage =
      @"{\"src\":\"file:///bogus\",\"referrerPolicy\":\"default\","
          "\"title\":\"Hello world!\"}";
  this->ImageTesterHelper(htmlForImage, expectedValueWhenClickingOnImage);
}

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, GetLinkImageUrlAtPoint) {
  NSString* htmlForImage =
      @"<a href='file:///linky'>"
          "<img id='foo' style='width:200;height:200;' src='file:///bogus'/>"
          "</a>";
  NSString* expectedValueWhenClickingOnImage =
      @"{\"src\":\"file:///bogus\",\"referrerPolicy\":\"default\","
          "\"href\":\"file:///linky\"}";
  this->ImageTesterHelper(htmlForImage, expectedValueWhenClickingOnImage);
}

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, TextAreaStopsProximity) {
  NSString* pageContent =
  @"<html><body style='margin-left:10px;margin-top:10px;'>"
  "<div style='width:100px;height:100px;'>"
  "<img id='foo'"
  "    style='position:absolute;left:0px;top:0px;width:50px;height:50px'"
  "    src='file:///bogus' />"
  "<input type='text' name='name'"
  "       style='position:absolute;left:5px;top:5px; width:40px;height:40px'/>"
  "</div></body> </html>";

  NSString* success = @"{\"src\":\"file:///bogus\","
      "\"referrerPolicy\":\"default\"}";
  NSString* failure = @"{}";

  TestScriptAndExpectedValue testData[] = {
    {
      @"__gCrWeb.getElementFromPoint(2, 20)",
      success
    },
    {
      @"__gCrWeb.getElementFromPoint(5, 20)",
      failure
    },
  };

  for (size_t i = 0; i < arraysize(testData); i++) {
    TestScriptAndExpectedValue& data = testData[i];
    this->LoadHtml(pageContent);
    NSString* result = this->RunJavaScript(data.testScript);
    EXPECT_NSEQ(data.expectedValue, result) << " in test " << i << ": " <<
    [data.testScript UTF8String];
  }
};

struct TestDataForPasswordFormDetection {
  NSString* pageContent;
  NSString* containsPassword;
};

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, HasPasswordField) {
  TestDataForPasswordFormDetection testData[] = {
    // Form without a password field.
    {
      @"<form><input type='text' name='password'></form>",
      @"false"
    },
    // Form with a password field.
    {
      @"<form><input type='password' name='password'></form>",
      @"true"
    }
  };
  for (size_t i = 0; i < arraysize(testData); i++) {
    TestDataForPasswordFormDetection& data = testData[i];
    this->LoadHtml(data.pageContent);
    NSString* result = this->RunJavaScript(@"__gCrWeb.hasPasswordField()");
    EXPECT_NSEQ(data.containsPassword, result) <<
        " in test " << i << ": " << [data.pageContent UTF8String];
  }
}

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, HasPasswordFieldinFrame) {
  TestDataForPasswordFormDetection data = {
    // Form with a password field in a nested iframe.
    @"<iframe name='pf'></iframe>"
    "<script>"
    "  var doc = frames['pf'].document.open();"
    "  doc.write('<form><input type=\\'password\\'></form>');"
    "  doc.close();"
    "</script>",
    @"true"
  };
  this->LoadHtml(data.pageContent);
  NSString* result = this->RunJavaScript(@"__gCrWeb.hasPasswordField()");
  EXPECT_NSEQ(data.containsPassword, result) << [data.pageContent UTF8String];
}

WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, Stringify) {
  // TODO(jeanfrancoisg): Test whether __gCrWeb.stringify(undefined) correctly
  //returns undefined.
  TestScriptAndExpectedValue testData[] = {
    // Stringify a string that contains various characters that must
    // be escaped.
    {
      @"__gCrWeb.stringify('a\\u000a\\t\\b\\\\\\\"Z')",
      @"\"a\\n\\t\\b\\\\\\\"Z\""
    },
    // Stringify a number.
    {
      @"__gCrWeb.stringify(77.7)",
      @"77.7"
    },
    // Stringify an array.
    {
      @"__gCrWeb.stringify(['a','b'])",
      @"[\"a\",\"b\"]"
    },
    // Stringify an object.
    {
      @"__gCrWeb.stringify({'a':'b','c':'d'})",
      @"{\"a\":\"b\",\"c\":\"d\"}"
    },
    // Stringify a hierarchy of objects and arrays.
    {
      @"__gCrWeb.stringify([{'a':['b','c'],'d':'e'},'f'])",
      @"[{\"a\":[\"b\",\"c\"],\"d\":\"e\"},\"f\"]"
    },
    // Stringify null.
    {
      @"__gCrWeb.stringify(null)",
      @"null"
    },
    // Stringify an object with a toJSON function.
    {
      @"temp = [1,2];"
      "temp.toJSON = function (key) {return undefined};"
      "__gCrWeb.stringify(temp)",
      @"[1,2]"
    },
    // Stringify an object with a toJSON property that is not a function.
    {
      @"temp = [1,2];"
      "temp.toJSON = 42;"
      "__gCrWeb.stringify(temp)",
      @"[1,2]"
    },
  };

  for (size_t i = 0; i < arraysize(testData); i++) {
    TestScriptAndExpectedValue& data = testData[i];
    // Load a sample HTML page. As a side-effect, loading HTML via
    // |webController_| will also inject core.js.
    this->LoadHtml(@"<p>");
    NSString* result = this->RunJavaScript(data.testScript);
    EXPECT_NSEQ(data.expectedValue, result) << " in test " << i << ": " <<
        [data.testScript UTF8String];
  }
}

// Tests the javascript of the url of the an image present in the DOM.
WEB_TEST_F(CoreJSUIWebViewTest, CoreJSWKWebViewTest, LinkOfImage) {
  // A page with a large image surrounded by a link.
  static const char image[] =
      "<a href='%s'><img width=400 height=400 src='foo'></img></a>";

  // A page with a link to a destination URL.
  this->LoadHtml(base::StringPrintf(image, "http://destination"));
  NSString* result =
      this->RunJavaScript(@"__gCrWeb['getElementFromPoint'](200, 200)");
  std::string expected_result =
      R"({"src":"foo","referrerPolicy":"default",)"
      R"("href":"http://destination/"})";
  EXPECT_EQ(expected_result, [result UTF8String]);

  // A page with a link with some JavaScript that does not result in a NOP.
  this->LoadHtml(base::StringPrintf(image,
                                    "javascript:console.log('whatever')"));
  result = this->RunJavaScript(@"__gCrWeb['getElementFromPoint'](200, 200)");
  expected_result =
      R"({"src":"foo","referrerPolicy":"default",)"
      R"("href":"javascript:console.log("})";
  EXPECT_EQ(expected_result, [result UTF8String]);

  // A list of JavaScripts that result in a NOP.
  std::vector<std::string> nop_javascripts;
  nop_javascripts.push_back(";");
  nop_javascripts.push_back("void(0);");
  nop_javascripts.push_back("void(0);  void(0); void(0)");

  for (auto js : nop_javascripts) {
    // A page with a link with some JavaScript that results in a NOP.
    const std::string javascript = std::string("javascript:") + js;
    this->LoadHtml(base::StringPrintf(image, javascript.c_str()));
    result = this->RunJavaScript(@"__gCrWeb['getElementFromPoint'](200, 200)");
    expected_result = R"({"src":"foo","referrerPolicy":"default"})";

    // Make sure the returned JSON does not have an 'href' key.
    EXPECT_EQ(expected_result, [result UTF8String]);
  }
}

}  // namespace
