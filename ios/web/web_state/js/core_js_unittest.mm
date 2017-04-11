// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

// Unit tests for ios/web/web_state/js/resources/core.js.

namespace {

struct TestScriptAndExpectedValue {
  NSString* test_script;
  id expected_value;
};

}  // namespace

namespace web {

// Test fixture to test core.js.
typedef web::WebTestWithWebState CoreJsTest;

struct TestDataForPasswordFormDetection {
  NSString* pageContent;
  NSNumber* containsPassword;
};

TEST_F(CoreJsTest, HasPasswordField) {
  TestDataForPasswordFormDetection testData[] = {
      // Form without a password field.
      {@"<form><input type='text' name='password'></form>", @NO},
      // Form with a password field.
      {@"<form><input type='password' name='password'></form>", @YES}};
  for (size_t i = 0; i < arraysize(testData); i++) {
    TestDataForPasswordFormDetection& data = testData[i];
    LoadHtml(data.pageContent);
    id result = ExecuteJavaScript(@"__gCrWeb.hasPasswordField()");
    EXPECT_NSEQ(data.containsPassword, result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.pageContent);
  }
}

TEST_F(CoreJsTest, HasPasswordFieldinFrame) {
  TestDataForPasswordFormDetection data = {
    // Form with a password field in a nested iframe.
    @"<iframe name='pf'></iframe>"
     "<script>"
     "  var doc = frames['pf'].document.open();"
     "  doc.write('<form><input type=\\'password\\'></form>');"
     "  doc.close();"
     "</script>",
    @YES
  };
  LoadHtml(data.pageContent);
  id result = ExecuteJavaScript(@"__gCrWeb.hasPasswordField()");
  EXPECT_NSEQ(data.containsPassword, result)
      << base::SysNSStringToUTF8(data.pageContent);
}

TEST_F(CoreJsTest, Stringify) {
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
    LoadHtml(@"<p>");
    id result = ExecuteJavaScript(data.test_script);
    EXPECT_NSEQ(data.expected_value, result)
        << " in test " << i << ": "
        << base::SysNSStringToUTF8(data.test_script);
  }
}

}  // namespace web
