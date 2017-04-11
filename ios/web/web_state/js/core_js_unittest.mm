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

}  // namespace web
