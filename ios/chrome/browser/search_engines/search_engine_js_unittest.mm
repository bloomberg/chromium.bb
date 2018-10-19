// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for search_engine.js testing.
class SearchEngineJsTest : public web::WebJsTest<web::WebTestWithWebState> {
 public:
  SearchEngineJsTest()
      : web::WebJsTest<web::WebTestWithWebState>(@[ @"search_engine" ]) {}

  DISALLOW_COPY_AND_ASSIGN(SearchEngineJsTest);
};

// Tests that __gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl returns
// the URL of the first OSDD found in page.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlSucceed) {
  LoadHtmlAndInject(
      @"<html><link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search' "
      @"href='//cs.chromium.org/codesearch/first_opensearch.xml' />"
      @"<link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search 2' "
      @"href='//cs.chromium.org/codesearch/second_opensearch.xml' />"
      @"<link href='/favicon.ico' rel='shortcut icon' "
      @"type='image/x-icon'></html>",
      GURL("https://cs.chromium.org"));

  id result = ExecuteJavaScript(
      @"__gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl();");
  EXPECT_NSEQ(@"https://cs.chromium.org/codesearch/first_opensearch.xml",
              result);
}

// Tests that __gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl returns
// undefined if no OSDD is found in page.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlFail) {
  LoadHtmlAndInject(
      @"<html><link href='/favicon.ico' rel='shortcut icon' "
      @"type='image/x-icon'></html>",
      GURL("https://cs.chromium.org"));

  id result = ExecuteJavaScript(
      @"__gCrWeb.searchEngine.getOpenSearchDescriptionDocumentUrl();");
  EXPECT_FALSE(result);
}
