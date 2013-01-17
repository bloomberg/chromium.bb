// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

class GDataWapiUrlGeneratorTest : public testing::Test {
 public:
  GDataWapiUrlGeneratorTest()
      : url_generator_(GURL(GDataWapiUrlGenerator::kBaseUrlForProduction)) {
  }

 protected:
  GDataWapiUrlGenerator url_generator_;
};

TEST_F(GDataWapiUrlGeneratorTest, AddStandardUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json",
            GDataWapiUrlGenerator::AddStandardUrlParams(
                GURL("http://www.example.com")).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, AddMetadataUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&include-installed-apps=true",
            GDataWapiUrlGenerator::AddMetadataUrlParams(
                GURL("http://www.example.com")).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, AddFeedUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true",
            GDataWapiUrlGenerator::AddFeedUrlParams(
                GURL("http://www.example.com"),
                100,  // num_items_to_fetch
                0,  // changestamp
                ""  // search_string
                                                 ).spec());
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true"
            "&start-index=123",
            GDataWapiUrlGenerator::AddFeedUrlParams(
                GURL("http://www.example.com"),
                100,  // num_items_to_fetch
                123,  // changestamp
                ""  // search_string
                                                 ).spec());
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showfolders=true"
            "&max-results=100"
            "&include-installed-apps=true"
            "&start-index=123"
            "&q=%22foo+bar%22",
            GDataWapiUrlGenerator::AddFeedUrlParams(
                GURL("http://www.example.com"),
                100,  // num_items_to_fetch
                123,  // changestamp
                "\"foo bar\""  // search_string
                                                 ).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceListUrl) {
  // This is the very basic URL for the GetResourceList operation.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/-/mine"
      "?v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             0,  // start_changestamp,
                                             "",  // search_string,
                                             false, // shared_with_me,
                                             ""  // directory resource ID
                                             ).spec());

  // With an override URL provided, the base URL is changed, but the default
  // parameters remain as-is.
  EXPECT_EQ(
      "http://localhost/"
      "?v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true",
      url_generator_.GenerateResourceListUrl(
          GURL("http://localhost/"),  // override_url,
          0,  // start_changestamp,
          "",  // search_string,
          false, // shared_with_me,
          ""  // directory resource ID
          ).spec());

  // With a non-zero start_changestamp provided, the base URL is changed from
  // "full/-/mine" to "changes", and "start-index" parameter is added.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/changes"
      "?v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true"
      "&start-index=100",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             100,  // start_changestamp,
                                             "",  // search_string,
                                             false, // shared_with_me,
                                             ""  // directory resource ID
                                             ).spec());

  // With a non-empty search string provided, "max-results" value is changed,
  // and "q" parameter is added.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/-/mine"
      "?v=3&alt=json&showfolders=true&max-results=50"
      "&include-installed-apps=true&q=foo",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             0,  // start_changestamp,
                                             "foo",  // search_string,
                                             false, // shared_with_me,
                                             ""  // directory resource ID
                                             ).spec());

  // With shared_with_me parameter set to true, the base URL is changed, but
  // the default parameters remain.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/-/shared-with-me"
      "?v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             0,  // start_changestamp,
                                             "",  // search_string,
                                             true, // shared_with_me,
                                             ""  // directory resource ID
                                             ).spec());

  // With a non-empty directory resource ID provided, the base URL is
  // changed, but the default parameters remain.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX/contents/-/mine"
      "?v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             0,  // start_changestamp,
                                             "",  // search_string,
                                             false, // shared_with_me,
                                             "XXX"  // directory resource ID
                                             ).spec());

  // With a non-empty override_url provided, the base URL is changed, but
  // the default parameters remain. Note that start-index should not be
  // overridden.
  EXPECT_EQ(
      "http://example.com/"
      "?start-index=123&v=3&alt=json&showfolders=true&max-results=500"
      "&include-installed-apps=true",
      url_generator_.GenerateResourceListUrl(
          GURL("http://example.com/?start-index=123"),  // override_url,
          100,  // start_changestamp,
          "",  // search_string,
          false, // shared_with_me,
          "XXX"  // directory resource ID
          ).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceEntryUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX?v=3&alt=json",
      url_generator_.GenerateResourceEntryUrl("XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceListRootUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full?v=3&alt=json",
      url_generator_.GenerateResourceListRootUrl().spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateAccountMetadataUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/metadata/default"
      "?v=3&alt=json&include-installed-apps=true",
      url_generator_.GenerateAccountMetadataUrl().spec());
}

}  // namespace google_apis
