// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_helper.h"

#include "base/stringprintf.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace {

class BrowsingDataHelperTest : public testing::Test {
 public:
  BrowsingDataHelperTest() {}
  virtual ~BrowsingDataHelperTest() {}

  bool IsWebScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasWebScheme(test) &&
            BrowsingDataHelper::IsWebScheme(scheme) &&
            BrowsingDataHelper::IsWebScheme(
                WebKit::WebString::fromUTF8(scheme)));
  }

  bool IsExtensionScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasExtensionScheme(test) &&
            BrowsingDataHelper::IsExtensionScheme(scheme) &&
            BrowsingDataHelper::IsExtensionScheme(
                WebKit::WebString::fromUTF8(scheme)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHelperTest);
};

TEST_F(BrowsingDataHelperTest, WebSafeSchemesAreWebSafe) {
  EXPECT_TRUE(IsWebScheme(chrome::kHttpScheme));
  EXPECT_TRUE(IsWebScheme(chrome::kHttpsScheme));
  EXPECT_TRUE(IsWebScheme(chrome::kFtpScheme));
  EXPECT_TRUE(IsWebScheme(chrome::kDataScheme));
  EXPECT_TRUE(IsWebScheme("feed"));
  EXPECT_TRUE(IsWebScheme(chrome::kBlobScheme));
  EXPECT_TRUE(IsWebScheme(chrome::kFileSystemScheme));
  EXPECT_FALSE(IsWebScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotWebSafe) {
  EXPECT_FALSE(IsWebScheme(chrome::kExtensionScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kAboutScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kChromeDevToolsScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kChromeInternalScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kChromeUIScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kJavaScriptScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kMailToScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kMetadataScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kSwappedOutScheme));
  EXPECT_FALSE(IsWebScheme(chrome::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, WebSafeSchemesAreNotExtensions) {
  EXPECT_FALSE(IsExtensionScheme(chrome::kHttpScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kHttpsScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kFtpScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kDataScheme));
  EXPECT_FALSE(IsExtensionScheme("feed"));
  EXPECT_FALSE(IsExtensionScheme(chrome::kBlobScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kFileSystemScheme));
  EXPECT_FALSE(IsExtensionScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotAllExtension) {
  EXPECT_TRUE(IsExtensionScheme(chrome::kExtensionScheme));

  EXPECT_FALSE(IsExtensionScheme(chrome::kAboutScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kChromeDevToolsScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kChromeInternalScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kChromeUIScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kJavaScriptScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kMailToScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kMetadataScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kSwappedOutScheme));
  EXPECT_FALSE(IsExtensionScheme(chrome::kViewSourceScheme));
}

}  // namespace
