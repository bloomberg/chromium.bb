// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_INT_TEST_H_
#define IOS_WEB_TEST_WEB_INT_TEST_H_

#import <WebKit/WebKit.h>

#include "ios/web/public/test/web_test.h"

namespace web {

// A test fixture for integration tests that need to bring up the HttpServer.
class WebIntTest : public WebTest {
 protected:
  WebIntTest();
  ~WebIntTest() override;

  // WebTest methods.
  void SetUp() override;
  void TearDown() override;

  // Synchronously removes data from |data_store|.
  // |websiteDataTypes| is from the constants defined in
  // "WebKit/WKWebsiteDataRecord".
  void RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                  NSSet* websiteDataTypes);

};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_TEST_H_
