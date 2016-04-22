// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_UTIL_H_

#import <map>

#include "url/gurl.h"

namespace web {
namespace test {

// Sets up a web::test::HttpServer with a simple HtmlResponseProvider. The
// HtmlResponseProvider will use the |responses| map to resolve URLs.
void SetUpSimpleHttpServer(std::map<GURL, std::string> responses);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_UTIL_H_