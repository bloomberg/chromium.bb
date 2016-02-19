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

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_UTIL_H_
