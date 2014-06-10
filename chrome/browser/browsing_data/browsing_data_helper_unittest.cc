// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_helper.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

const int kExtension = BrowsingDataHelper::EXTENSION;
const int kProtected = BrowsingDataHelper::PROTECTED_WEB;
const int kUnprotected = BrowsingDataHelper::UNPROTECTED_WEB;

class BrowsingDataHelperTest : public testing::Test {
 public:
  BrowsingDataHelperTest() {}
  virtual ~BrowsingDataHelperTest() {}

  bool IsWebScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasWebScheme(test) &&
            BrowsingDataHelper::IsWebScheme(scheme));
  }

  bool IsExtensionScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (BrowsingDataHelper::HasExtensionScheme(test) &&
            BrowsingDataHelper::IsExtensionScheme(scheme));
  }

  bool Match(const GURL& origin,
             int mask,
             ExtensionSpecialStoragePolicy* policy) {
    return BrowsingDataHelper::DoesOriginMatchMask(origin, mask, policy);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHelperTest);
};

TEST_F(BrowsingDataHelperTest, WebSafeSchemesAreWebSafe) {
  EXPECT_TRUE(IsWebScheme(url::kHttpScheme));
  EXPECT_TRUE(IsWebScheme(url::kHttpsScheme));
  EXPECT_TRUE(IsWebScheme(url::kFtpScheme));
  EXPECT_TRUE(IsWebScheme(url::kDataScheme));
  EXPECT_TRUE(IsWebScheme("feed"));
  EXPECT_TRUE(IsWebScheme(url::kBlobScheme));
  EXPECT_TRUE(IsWebScheme(url::kFileSystemScheme));
  EXPECT_FALSE(IsWebScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotWebSafe) {
  EXPECT_FALSE(IsWebScheme(extensions::kExtensionScheme));
  EXPECT_FALSE(IsWebScheme(url::kAboutScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsWebScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsWebScheme(url::kMailToScheme));
  EXPECT_FALSE(IsWebScheme(content::kMetadataScheme));
  EXPECT_FALSE(IsWebScheme(content::kSwappedOutScheme));
  EXPECT_FALSE(IsWebScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, WebSafeSchemesAreNotExtensions) {
  EXPECT_FALSE(IsExtensionScheme(url::kHttpScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kHttpsScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kFtpScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kDataScheme));
  EXPECT_FALSE(IsExtensionScheme("feed"));
  EXPECT_FALSE(IsExtensionScheme(url::kBlobScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kFileSystemScheme));
  EXPECT_FALSE(IsExtensionScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotAllExtension) {
  EXPECT_TRUE(IsExtensionScheme(extensions::kExtensionScheme));

  EXPECT_FALSE(IsExtensionScheme(url::kAboutScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsExtensionScheme(url::kMailToScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kMetadataScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kSwappedOutScheme));
  EXPECT_FALSE(IsExtensionScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, TestMatches) {
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  // Protect kOrigin1.
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  EXPECT_FALSE(Match(kOrigin1, kUnprotected, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(kOrigin1, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(kOrigin1, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kProtected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(
      kOrigin1, kUnprotected | kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(
      kOrigin2, kUnprotected | kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(
      kOriginExt, kUnprotected | kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools,
                     kUnprotected | kProtected | kExtension,
                     mock_policy.get()));
}

}  // namespace
