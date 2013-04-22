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
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showroot=true",
            GDataWapiUrlGenerator::AddStandardUrlParams(
                GURL("http://www.example.com")).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, AddInitiateUploadUrlParams) {
  EXPECT_EQ("http://www.example.com/?convert=false&v=3&alt=json&showroot=true",
            GDataWapiUrlGenerator::AddInitiateUploadUrlParams(
                GURL("http://www.example.com")).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, AddFeedUrlParams) {
  EXPECT_EQ(
      "http://www.example.com/?v=3&alt=json&showroot=true&"
      "showfolders=true"
      "&include-shared=true"
      "&max-results=100",
      GDataWapiUrlGenerator::AddFeedUrlParams(GURL("http://www.example.com"),
                                              100  // num_items_to_fetch
                                              ).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceListUrl) {
  // This is the very basic URL for the GetResourceList operation.
  EXPECT_EQ("https://docs.google.com/feeds/default/private/full"
            "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
            "&max-results=500",
            url_generator_.GenerateResourceListUrl(
                GURL(),         // override_url,
                0,              // start_changestamp,
                std::string(),  // search_string,
                std::string()   // directory resource ID
                ).spec());

  // With an override URL provided, the base URL is changed, but the default
  // parameters remain as-is.
  EXPECT_EQ("http://localhost/"
            "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
            "&max-results=500",
            url_generator_.GenerateResourceListUrl(
                GURL("http://localhost/"),  // override_url,
                0,                          // start_changestamp,
                std::string(),              // search_string,
                std::string()               // directory resource ID
                ).spec());

  // With a non-zero start_changestamp provided, the base URL is changed from
  // "full" to "changes", and "start-index" parameter is added.
  EXPECT_EQ("https://docs.google.com/feeds/default/private/changes"
            "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
            "&max-results=500&start-index=100",
            url_generator_.GenerateResourceListUrl(
                GURL(),         // override_url,
                100,            // start_changestamp,
                std::string(),  // search_string,
                std::string()   // directory resource ID
                ).spec());

  // With a non-empty search string provided, "max-results" value is changed,
  // and "q" parameter is added.
  EXPECT_EQ("https://docs.google.com/feeds/default/private/full"
            "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
            "&max-results=50&q=foo",
            url_generator_.GenerateResourceListUrl(
                GURL(),        // override_url,
                0,             // start_changestamp,
                "foo",         // search_string,
                std::string()  // directory resource ID
                ).spec());

  // With a non-empty directory resource ID provided, the base URL is
  // changed, but the default parameters remain.
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX/contents"
      "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
      "&max-results=500",
      url_generator_.GenerateResourceListUrl(GURL(),  // override_url,
                                             0,       // start_changestamp,
                                             std::string(),  // search_string,
                                             "XXX"  // directory resource ID
                                             ).spec());

  // With a non-empty override_url provided, the base URL is changed, but
  // the default parameters remain. Note that start-index should not be
  // overridden.
  EXPECT_EQ("http://example.com/"
            "?start-index=123&v=3&alt=json&showroot=true&showfolders=true"
            "&include-shared=true&max-results=500",
            url_generator_.GenerateResourceListUrl(
                GURL("http://example.com/?start-index=123"),  // override_url,
                100,            // start_changestamp,
                std::string(),  // search_string,
                "XXX"           // directory resource ID
                ).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateSearchByTitleUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full"
      "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
      "&max-results=500&title=search-title&title-exact=true",
      url_generator_.GenerateSearchByTitleUrl(
          "search-title", std::string()).spec());

  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX/contents"
      "?v=3&alt=json&showroot=true&showfolders=true&include-shared=true"
      "&max-results=500&title=search-title&title-exact=true",
      url_generator_.GenerateSearchByTitleUrl(
          "search-title", "XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateEditUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX?v=3&alt=json"
      "&showroot=true",
      url_generator_.GenerateEditUrl("XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateEditUrlWithoutParams) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX",
      url_generator_.GenerateEditUrlWithoutParams("XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateContentUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/"
      "folder%3Aroot/contents?v=3&alt=json&showroot=true",
      url_generator_.GenerateContentUrl("folder:root").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceUrlForRemoval) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/"
      "folder%3Aroot/contents/file%3AABCDE?v=3&alt=json&showroot=true",
      url_generator_.GenerateResourceUrlForRemoval(
          "folder:root", "file:ABCDE").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateInitiateUploadNewFileUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/upload/create-session/default/private/"
      "full/folder%3Aabcde/contents?convert=false&v=3&alt=json&showroot=true",
      url_generator_.GenerateInitiateUploadNewFileUrl("folder:abcde").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateInitiateUploadExistingFileUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/upload/create-session/default/private/"
      "full/file%3Aresource_id?convert=false&v=3&alt=json&showroot=true",
      url_generator_.GenerateInitiateUploadExistingFileUrl(
          "file:resource_id").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateResourceListRootUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full?v=3&alt=json"
      "&showroot=true",
      url_generator_.GenerateResourceListRootUrl().spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateAccountMetadataUrl) {
  // Include installed apps.
  EXPECT_EQ(
      "https://docs.google.com/feeds/metadata/default"
      "?v=3&alt=json&showroot=true&include-installed-apps=true",
      url_generator_.GenerateAccountMetadataUrl(true).spec());

  // Exclude installed apps.
  EXPECT_EQ(
      "https://docs.google.com/feeds/metadata/default?v=3&alt=json"
      "&showroot=true",
      url_generator_.GenerateAccountMetadataUrl(false).spec());
}

}  // namespace google_apis
