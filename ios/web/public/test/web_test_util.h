// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_UTIL_H_

#include "ios/web/public/web_view_creation_util.h"

// A helper macro that allows skipping a unit test on iOS7 and earlier. Example:
//
// TEST_F(WKWebViewTest, WebViewInitializesCorrectly) {
//   CR_TEST_REQUIRES_WK_WEB_VIEW();
//   EXPECT_TRUE(NSClassFromString(@"WKWebView") != nil);
// }
#define CR_TEST_REQUIRES_WK_WEB_VIEW() \
  if (!web::IsWKWebViewSupported())    \
  return

// Defines a web test that uses two test fixtures and single testing code.
//
// The first two parameters are the corresponding names of UIWebView-based and
// WKWebView-based test fixture classes. Those will also be the test case names.
// The third parameter is the name of the test within the test case.
//
// A test fixture class must be declared earlier. Test code goes between braces
// after using this macro. Example:
//
// typedef web::WebTest UIViewTest;
// typedef web::WKWebViewWebTest WKViewTest;
//
// WEB_TEST_F(UIViewTest, WKViewTest, ControllerInitializesCorrectly) {
//   EXPECT_TRUE(this->webController_);
// }

// Makes the name of a parent test class for WK and UI test fixtures.
// Only intended for use by WEB_TEST_F, not for direct use.
#define WEB_TEST_BASE_CLASS_(ui_fixture, wk_test_case, test_name)\
  ui_fixture##wk_test_case##_##test_name##_WebTest

// Makes the name of a concrete WK or UI test fixture.
// Only intended for use by WEB_TEST_F, not for direct use.
#define WEB_TEST_TEST_CLASS_(test_case, test_name)\
  test_case##_##test_name##_WebTest

// Makes the testing template class that runs WebTestBody function as the
// testing code.
// Only intended for use by WEB_TEST_F, not for direct use.
#define GTEST_WEB_TEST_(test_case, test_name, is_wk_web_view) \
  GTEST_TEST_(test_case, test_name,                           \
              WEB_TEST_TEST_CLASS_(test_case, test_name),     \
              ::testing::internal::GetTypeId<test_case>()) {  \
    if (!is_wk_web_view || web::IsWKWebViewSupported())       \
      WebTestBody();                                          \
  };

#define WEB_TEST_F(ui_fixture, wk_fixture, test_name)                      \
  template <typename T>                                                    \
  class WEB_TEST_BASE_CLASS_(ui_fixture, wk_fixture, test_name)            \
      : public T {                                                         \
   protected:                                                              \
    void WebTestBody();                                                    \
  };                                                                       \
  typedef WEB_TEST_BASE_CLASS_(                                            \
      ui_fixture, wk_fixture,                                              \
      test_name) <ui_fixture> WEB_TEST_TEST_CLASS_(ui_fixture, test_name); \
  typedef WEB_TEST_BASE_CLASS_(                                            \
      ui_fixture, wk_fixture,                                              \
      test_name) <wk_fixture> WEB_TEST_TEST_CLASS_(wk_fixture, test_name); \
  GTEST_WEB_TEST_(ui_fixture, test_name, false)                            \
  GTEST_WEB_TEST_(wk_fixture, test_name, true)                             \
  template <typename T>                                                    \
  void WEB_TEST_BASE_CLASS_(ui_fixture, wk_fixture,                        \
                            test_name) <T>::WebTestBody()

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_UTIL_H_
