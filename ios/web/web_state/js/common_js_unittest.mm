// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

// Struct for isTextField() test data.
struct TextFieldTestElement {
  // The element name.
  const char* element_name;
  // The index of this element in those that have the same name.
  const int element_index;
  // True if this is expected to be a text field.
  const bool expected_is_text_field;
};

}  // namespace

namespace web {

// Test fixture to test common.js.
typedef web::WebTestWithWebState CommonJsTest;

// Tests __gCrWeb.common.isTextField JavaScript API.
TEST_F(CommonJsTest, IsTestField) {
  LoadHtml(@"<html><body>"
            "<input type='text' name='firstname'>"
            "<input type='text' name='lastname'>"
            "<input type='email' name='email'>"
            "<input type='tel' name='phone'>"
            "<input type='url' name='blog'>"
            "<input type='number' name='expected number of clicks'>"
            "<input type='password' name='pwd'>"
            "<input type='checkbox' name='vehicle' value='Bike'>"
            "<input type='checkbox' name='vehicle' value='Car'>"
            "<input type='checkbox' name='vehicle' value='Rocket'>"
            "<input type='radio' name='boolean' value='true'>"
            "<input type='radio' name='boolean' value='false'>"
            "<input type='radio' name='boolean' value='other'>"
            "<select name='state'>"
            "  <option value='CA'>CA</option>"
            "  <option value='MA'>MA</option>"
            "</select>"
            "<select name='cars' multiple>"
            "  <option value='volvo'>Volvo</option>"
            "  <option value='saab'>Saab</option>"
            "  <option value='opel'>Opel</option>"
            "  <option value='audi'>Audi</option>"
            "</select>"
            "<input type='submit' name='submit' value='Submit'>"
            "</body></html>");

  static const struct TextFieldTestElement testElements[] = {
      {"firstname", 0, true},
      {"lastname", 0, true},
      {"email", 0, true},
      {"phone", 0, true},
      {"blog", 0, true},
      {"expected number of clicks", 0, true},
      {"pwd", 0, true},
      {"vehicle", 0, false},
      {"vehicle", 1, false},
      {"vehicle", 2, false},
      {"boolean", 0, false},
      {"boolean", 1, false},
      {"boolean", 2, false},
      {"state", 0, false},
      {"cars", 0, false},
      {"submit", 0, false}};
  for (size_t i = 0; i < arraysize(testElements); ++i) {
    TextFieldTestElement element = testElements[i];
    id result = ExecuteJavaScript([NSString
        stringWithFormat:@"__gCrWeb.common.isTextField("
                          "window.document.getElementsByName('%s')[%d])",
                         element.element_name, element.element_index]);
    EXPECT_NSEQ(element.expected_is_text_field ? @YES : @NO, result)
        << element.element_name << " with index " << element.element_index
        << " isTextField(): " << element.expected_is_text_field;
  }
}

}  // namespace web
