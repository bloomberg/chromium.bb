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

  bool IsValidScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasValidScheme(test) &&
            BrowsingDataHelper::IsValidScheme(scheme) &&
            BrowsingDataHelper::IsValidScheme(
                WebKit::WebString::fromUTF8(scheme)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHelperTest);
};

TEST_F(BrowsingDataHelperTest, WebSafeSchemesAreValid) {
  EXPECT_TRUE(IsValidScheme(chrome::kHttpScheme));
  EXPECT_TRUE(IsValidScheme(chrome::kHttpsScheme));
  EXPECT_TRUE(IsValidScheme(chrome::kFtpScheme));
  EXPECT_TRUE(IsValidScheme(chrome::kDataScheme));
  EXPECT_TRUE(IsValidScheme("feed"));
  EXPECT_TRUE(IsValidScheme(chrome::kBlobScheme));
  EXPECT_TRUE(IsValidScheme(chrome::kFileSystemScheme));

  EXPECT_FALSE(IsValidScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreInvalid) {
  EXPECT_FALSE(IsValidScheme(chrome::kExtensionScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kAboutScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kChromeDevToolsScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kChromeInternalScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kChromeUIScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kJavaScriptScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kMailToScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kMetadataScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kSwappedOutScheme));
  EXPECT_FALSE(IsValidScheme(chrome::kViewSourceScheme));
}

}  // namespace
