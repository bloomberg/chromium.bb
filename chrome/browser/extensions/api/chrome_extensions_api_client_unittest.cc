// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(TestChromeExtensionsAPIClient, ShouldHideResponseHeader) {
  ChromeExtensionsAPIClient client;
  EXPECT_TRUE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "X-Chrome-ID-Consistency-Response"));
  EXPECT_TRUE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "x-cHroMe-iD-CoNsiStenCY-RESPoNSE"));
  EXPECT_FALSE(client.ShouldHideResponseHeader(
      GURL("http://www.example.com"), "X-Chrome-ID-Consistency-Response"));
  EXPECT_FALSE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "Google-Accounts-SignOut"));
}

}  // namespace extensions
