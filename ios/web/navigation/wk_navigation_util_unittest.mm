// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/test/test_url_constants.h"
#include "net/base/escape.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace wk_navigation_util {

typedef PlatformTest WKNavigationUtilTest;

TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrl) {
  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(GURL("http://www.0.com"));
  item0->SetTitle(base::ASCIIToUTF16("Test Website 0"));
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(GURL("http://www.1.com"));
  // Create an App-specific URL
  auto item2 = std::make_unique<NavigationItemImpl>();
  GURL url2("http://webui");
  GURL::Replacements scheme_replacements;
  scheme_replacements.SetSchemeStr(kTestWebUIScheme);
  item2->SetURL(url2.ReplaceComponents(scheme_replacements));

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));
  items.push_back(std::move(item2));

  GURL restore_session_url =
      CreateRestoreSessionUrl(0 /* last_committed_item_index */, items);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  net::UnescapeRule::Type unescape_rules =
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS;
  std::string session_json =
      net::UnescapeURLComponent(restore_session_url.ref(), unescape_rules);
  EXPECT_EQ(
      "session={\"offset\":-2,\"titles\":[\"Test Website 0\",\"\",\"\"],"
      "\"urls\":[\"http://www.0.com/\",\"http://www.1.com/\","
      "\"about:blank?for=testwebui%3A%2F%2Fwebui%2F\"]}",
      session_json);
}

// Verifies that large session can be stored in NSURL. GURL is converted to
// NSURL, because NSURL is passed to WKWebView during the session restoration.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrlForLargeSession) {
  // Create restore session URL with large number of items.
  const size_t kItemCount = 5000;
  std::vector<std::unique_ptr<NavigationItem>> items;
  for (size_t i = 0; i < kItemCount; i++) {
    auto item = std::make_unique<NavigationItemImpl>();
    item->SetURL(GURL(base::StringPrintf("http://www.%zu.com", i)));
    item->SetTitle(base::ASCIIToUTF16(base::StringPrintf("Test%zu", i)));
    items.push_back(std::move(item));
  }
  GURL restore_session_url =
      CreateRestoreSessionUrl(/*last_committed_item_index=*/0, items);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  // Extract session JSON from restoration URL.
  NSString* fragment = net::NSURLWithGURL(restore_session_url).fragment;
  NSString* encoded_session = [fragment substringFromIndex:strlen("session=")];
  std::string session_json = net::UnescapeURLComponent(
      base::SysNSStringToUTF8(encoded_session),
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

  // Parse JSON to get the session dictionary.
  int error_code = 0;
  std::unique_ptr<base::Value> session_value(
      base::JSONReader::ReadAndReturnError(session_json, base::JSON_PARSE_RFC,
                                           &error_code, nullptr));
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(session_value.get());

  // Verify that all titles and URLs are present.
  base::Value* titles_value = session_value->FindKey("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_TRUE(titles_value->is_list());
  ASSERT_EQ(kItemCount, titles_value->GetList().size());

  base::Value* urls_value = session_value->FindKey("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_TRUE(urls_value->is_list());
  ASSERT_EQ(kItemCount, urls_value->GetList().size());
}

TEST_F(WKNavigationUtilTest, IsNotRestoreSessionUrl) {
  EXPECT_FALSE(IsRestoreSessionUrl(GURL()));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("file://somefile")));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("http://www.1.com")));
}

// Tests that CreateRedirectUrl and ExtractTargetURL used back-to-back is an
// identity transformation.
TEST_F(WKNavigationUtilTest, CreateAndExtractTargetURL) {
  GURL target_url = GURL("http://www.1.com?query=special%26chars");
  GURL url = CreateRedirectUrl(target_url);
  ASSERT_TRUE(url.SchemeIsFile());

  GURL extracted_url;
  ASSERT_TRUE(ExtractTargetURL(url, &extracted_url));
  EXPECT_EQ(target_url, extracted_url);
}

TEST_F(WKNavigationUtilTest, IsPlaceholderUrl) {
  // Valid placeholder URLs.
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=")));
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=chrome%3A%2F%2Fnewtab")));

  // Not an about:blank URL.
  EXPECT_FALSE(IsPlaceholderUrl(GURL::EmptyGURL()));
  // Missing ?for= query parameter.
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank")));
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank?chrome:%3A%2F%2Fnewtab")));
}

TEST_F(WKNavigationUtilTest, EncodReturnsEmptyOnInvalidUrls) {
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL::EmptyGURL()));
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL("notaurl")));
}

TEST_F(WKNavigationUtilTest, EncodeDecodeValidUrls) {
  {
    GURL original("chrome://chrome-urls");
    GURL encoded("about:blank?for=chrome%3A%2F%2Fchrome-urls");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
  {
    GURL original("about:blank");
    GURL encoded("about:blank?for=about%3Ablank");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
}

// Tests that invalid URLs will be rejected in decoding.
TEST_F(WKNavigationUtilTest, DecodeRejectInvalidUrls) {
  GURL encoded("about:blank?for=thisisnotanurl");
  EXPECT_EQ(GURL::EmptyGURL(), ExtractUrlFromPlaceholderUrl(encoded));
}

}  // namespace wk_navigation_util
}  // namespace web
