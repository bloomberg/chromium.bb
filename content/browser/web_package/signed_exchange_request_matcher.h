// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "content/browser/web_package/http_structured_header.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SignedExchangeRequestMatcher {
 public:
  // Keys must be lower-cased.
  using HeaderMap = std::map<std::string, std::string>;

  static bool MatchRequest(const HeaderMap& request_headers,
                           const HeaderMap& response_headers);

 private:
  static std::vector<std::vector<std::string>> CacheBehavior(
      const http_structured_header::ListOfLists& variants,
      const HeaderMap& request_headers);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeRequestMatcherTest, CacheBehavior);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_
