// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_url_generator.h"

#include "chrome/browser/google_apis/test_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

class DriveApiUrlGeneratorTest : public testing::Test {
 public:
  DriveApiUrlGeneratorTest()
      : url_generator_(GURL(DriveApiUrlGenerator::kBaseUrlForProduction)),
        test_url_generator_(test_util::GetBaseUrlForTesting(12345)) {
  }

 protected:
  DriveApiUrlGenerator url_generator_;
  DriveApiUrlGenerator test_url_generator_;
};

// Make sure the hard-coded urls are returned.
TEST_F(DriveApiUrlGeneratorTest, GetAboutUrl) {
  EXPECT_EQ("https://www.googleapis.com/drive/v2/about",
            url_generator_.GetAboutUrl().spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/about",
            test_url_generator_.GetAboutUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetApplistUrl) {
  EXPECT_EQ("https://www.googleapis.com/drive/v2/apps",
            url_generator_.GetApplistUrl().spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/apps",
            test_url_generator_.GetApplistUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChangelistUrl) {
  // Use default URL, if |override_url| is empty.
  // Do not add startChangeId parameter if |start_changestamp| is 0.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes",
            url_generator_.GetChangelistUrl(GURL(), 0).spec());

  // Set startChangeId parameter if |start_changestamp| is given.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes?startChangeId=100",
            url_generator_.GetChangelistUrl(GURL(), 100).spec());

  // Use the |override_url| for the base URL if given.
  // The behavior for the |start_changestamp| should be as same as above cases.
  EXPECT_EQ("https://localhost/drive/v2/changes",
            url_generator_.GetChangelistUrl(
                GURL("https://localhost/drive/v2/changes"), 0).spec());
  EXPECT_EQ("https://localhost/drive/v2/changes?startChangeId=200",
            url_generator_.GetChangelistUrl(
                GURL("https://localhost/drive/v2/changes"), 200).spec());

  // For test server, the given base url should be used,
  // but if |override_url| is given, |override_url| should be used.
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/changes?startChangeId=100",
            test_url_generator_.GetChangelistUrl(GURL(), 100).spec());
  EXPECT_EQ("https://localhost/drive/v2/changes?startChangeId=200",
            test_url_generator_.GetChangelistUrl(
                GURL("https://localhost/drive/v2/changes"), 200).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilelistUrl) {
  // Use default URL, if |override_url| is empty.
  // Do not add q parameter if |search_string| is empty.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files",
            url_generator_.GetFilelistUrl(GURL(), "").spec());

  // Set q parameter if non-empty |search_string| is given.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files?q=query",
            url_generator_.GetFilelistUrl(GURL(), "query").spec());

  // Use the |override_url| for the base URL if given.
  // The behavior for the |search_string| should be as same as above cases.
  EXPECT_EQ("https://localhost/drive/v2/files",
            url_generator_.GetFilelistUrl(
                GURL("https://localhost/drive/v2/files"), "").spec());
  EXPECT_EQ("https://localhost/drive/v2/files?q=query",
            url_generator_.GetFilelistUrl(
                GURL("https://localhost/drive/v2/files"), "query").spec());

  // For test server, the given base url should be used,
  // but if |override_url| is given, |override_url| should be used.
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files?q=query",
            test_url_generator_.GetFilelistUrl(GURL(), "query").spec());
  EXPECT_EQ("https://localhost/drive/v2/files?q=query",
            test_url_generator_.GetFilelistUrl(
                GURL("https://localhost/drive/v2/files"), "query").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFileUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0ADK06pfg",
            url_generator_.GetFileUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0Bz0bd074",
            url_generator_.GetFileUrl("0Bz0bd074").spec());

  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0ADK06pfg",
            test_url_generator_.GetFileUrl("0ADK06pfg").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0Bz0bd074",
            test_url_generator_.GetFileUrl("0Bz0bd074").spec());
}

}  // namespace google_apis
