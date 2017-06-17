// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_cookie_store_wrapper.h"

#include <memory>

#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_store_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

struct AwCookieStoreWrapperTestTraits {
  static std::unique_ptr<net::CookieStore> Create() {
    std::unique_ptr<net::CookieStore> cookie_store(new AwCookieStoreWrapper());

    // Android Webview can run multiple tests without restarting the binary,
    // so have to delete any cookies the global store may have from an earlier
    // test.
    net::ResultSavingCookieCallback<int> callback;
    cookie_store->DeleteAllAsync(
        base::Bind(&net::ResultSavingCookieCallback<int>::Run,
                   base::Unretained(&callback)));
    callback.WaitUntilDone();

    return cookie_store;
  }

  static const bool supports_http_only = true;
  static const bool supports_non_dotted_domains = true;
  static const bool preserves_trailing_dots = true;
  static const bool filters_schemes = true;
  static const bool has_path_prefix_bug = false;
  static const bool forbids_setting_empty_name = false;
  static const int creation_time_granularity_in_ms = 0;
};

}  // namespace android_webview

// Run the standard cookie tests with AwCookieStoreWrapper. Macro must be in
// net namespace.
namespace net {
INSTANTIATE_TYPED_TEST_CASE_P(AwCookieStoreWrapper,
                              CookieStoreTest,
                              android_webview::AwCookieStoreWrapperTestTraits);
}  // namespace net
