// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_int_test.h"

#include "base/ios/block_types.h"

#include "base/test/ios/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/web_view_creation_util.h"

namespace web {

WebIntTest::WebIntTest() {}
WebIntTest::~WebIntTest() {}

void WebIntTest::SetUp() {
  WebTest::SetUp();

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  ASSERT_FALSE(server.IsRunning());
  server.StartOrDie();

  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);
}

void WebIntTest::TearDown() {
  RemoveWKWebViewCreatedData([WKWebsiteDataStore defaultDataStore],
                             [WKWebsiteDataStore allWebsiteDataTypes]);

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.Stop();
  EXPECT_FALSE(server.IsRunning());

  WebTest::TearDown();
}

void WebIntTest::RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                            NSSet* websiteDataTypes) {
  __block bool data_removed = false;

  ProceduralBlock remove_data = ^{
    [data_store removeDataOfTypes:websiteDataTypes
                    modifiedSince:[NSDate distantPast]
                completionHandler:^{
                  data_removed = true;
                }];
  };

  if ([websiteDataTypes containsObject:WKWebsiteDataTypeCookies]) {
    // TODO(crbug.com/554225): This approach of creating a WKWebView and
    // executing JS to clear cookies is a workaround for
    // https://bugs.webkit.org/show_bug.cgi?id=149078.
    // Remove this, when that bug is fixed. The |markerWKWebView| will be
    // released when cookies have been cleared.
    WKWebView* marker_web_view =
        web::CreateWKWebView(CGRectZero, GetBrowserState());
    [marker_web_view evaluateJavaScript:@""
                      completionHandler:^(id, NSError*) {
                        [marker_web_view release];
                        remove_data();
                      }];
  } else {
    remove_data();
  }

  base::test::ios::WaitUntilCondition(^bool {
    return data_removed;
  });
}

}  // namespace web
