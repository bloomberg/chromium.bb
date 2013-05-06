// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace extensions {

namespace errors = externally_connectable_errors;

class ExternallyConnectableTest : public ExtensionManifestTest {
 public:
  ExternallyConnectableTest() : channel_(chrome::VersionInfo::CHANNEL_DEV) {}

 private:
  Feature::ScopedCurrentChannel channel_;
};

TEST_F(ExternallyConnectableTest, IDsAndMatches) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("externally_connectable_ids_and_matches.json");
  ASSERT_TRUE(extension);

  ExternallyConnectableInfo* info = ExternallyConnectableInfo::Get(extension);
  ASSERT_TRUE(info);

  EXPECT_THAT(info->ids, ElementsAre("abcdefghijklmnopabcdefghijklmnop",
                                     "ponmlkjihgfedcbaponmlkjihgfedcba"));

  EXPECT_FALSE(info->matches_all_ids);

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://example.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://example.com/")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://example.com/index.html")));

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com/index.html")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("https://google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("https://google.com/")));

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://build.chromium.org")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://build.chromium.org/")));
  EXPECT_TRUE(
      info->matches.MatchesURL(GURL("http://build.chromium.org/index.html")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("https://build.chromium.org")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("https://build.chromium.org/")));
  EXPECT_FALSE(
      info->matches.MatchesURL(GURL("http://foo.chromium.org/index.html")));

  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://yahoo.com")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://yahoo.com/")));
}

TEST_F(ExternallyConnectableTest, IDs) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("externally_connectable_ids.json");
  ASSERT_TRUE(extension);

  ExternallyConnectableInfo* info = ExternallyConnectableInfo::Get(extension);
  ASSERT_TRUE(info);

  EXPECT_THAT(info->ids, ElementsAre("abcdefghijklmnopabcdefghijklmnop",
                                     "ponmlkjihgfedcbaponmlkjihgfedcba"));

  EXPECT_FALSE(info->matches_all_ids);

  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://google.com/index.html")));
}

TEST_F(ExternallyConnectableTest, Matches) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("externally_connectable_matches.json");
  ASSERT_TRUE(extension);

  ExternallyConnectableInfo* info = ExternallyConnectableInfo::Get(extension);
  ASSERT_TRUE(info);

  EXPECT_THAT(info->ids, ElementsAre());

  EXPECT_FALSE(info->matches_all_ids);

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://example.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://example.com/")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://example.com/index.html")));

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://google.com/index.html")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("https://google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("https://google.com/")));

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://build.chromium.org")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://build.chromium.org/")));
  EXPECT_TRUE(
      info->matches.MatchesURL(GURL("http://build.chromium.org/index.html")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("https://build.chromium.org")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("https://build.chromium.org/")));
  EXPECT_FALSE(
      info->matches.MatchesURL(GURL("http://foo.chromium.org/index.html")));

  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://yahoo.com")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://yahoo.com/")));
}

TEST_F(ExternallyConnectableTest, AllIDs) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("externally_connectable_all_ids.json");
  ASSERT_TRUE(extension);

  ExternallyConnectableInfo* info = ExternallyConnectableInfo::Get(extension);
  ASSERT_TRUE(info);

  EXPECT_THAT(info->ids, ElementsAre("abcdefghijklmnopabcdefghijklmnop",
                                     "ponmlkjihgfedcbaponmlkjihgfedcba"));

  EXPECT_TRUE(info->matches_all_ids);

  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://google.com/index.html")));
}

TEST_F(ExternallyConnectableTest, ErrorWrongFormat) {
  RunTestcase(Testcase("externally_connectable_error_wrong_format.json",
                       errors::kErrorInvalid),
              EXPECT_TYPE_ERROR);
}

TEST_F(ExternallyConnectableTest, ErrorBadID) {
  RunTestcase(Testcase("externally_connectable_bad_id.json",
                       ErrorUtils::FormatErrorMessage(errors::kErrorInvalidId,
                                                      "badid")),
              EXPECT_TYPE_ERROR);
}

TEST_F(ExternallyConnectableTest, ErrorBadMatches) {
  RunTestcase(
      Testcase("externally_connectable_error_bad_matches.json",
               ErrorUtils::FormatErrorMessage(errors::kErrorInvalidMatchPattern,
                                              "www.yahoo.com")),
      EXPECT_TYPE_ERROR);
}

}  // namespace extensions
