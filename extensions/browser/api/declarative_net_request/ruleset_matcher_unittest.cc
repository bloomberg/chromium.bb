// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

class RulesetMatcherTest : public ::testing::Test {
 public:
  RulesetMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 protected:
  // Helper to create a verified ruleset matcher. Populates |matcher| and
  // |expected_checksum|.
  void CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                             const RulesetSource& source,
                             std::unique_ptr<RulesetMatcher>* matcher,
                             int* expected_checksum = nullptr) {
    // Serialize |rules|.
    ListBuilder builder;
    for (const auto& rule : rules)
      builder.Append(rule.ToValue());
    JSONFileValueSerializer(source.json_path()).Serialize(*builder.Build());

    // Index ruleset.
    IndexAndPersistRulesResult result = source.IndexAndPersistRulesUnsafe();
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.error.empty());

    if (expected_checksum)
      *expected_checksum = result.ruleset_checksum;

    // Create verified matcher.
    RulesetMatcher::LoadRulesetResult load_result =
        RulesetMatcher::CreateVerifiedMatcher(source.indexed_path(),
                                              result.ruleset_checksum, matcher);
    ASSERT_EQ(RulesetMatcher::kLoadSuccess, load_result);
  }

  RulesetSource CreateTemporarySource() {
    base::FilePath json_path;
    base::FilePath indexed_path;
    CHECK(base::CreateTemporaryFile(&json_path));
    CHECK(base::CreateTemporaryFile(&indexed_path));
    return RulesetSource(std::move(json_path), std::move(indexed_path));
  }

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcherTest);
};

// Tests a simple blocking rule.
TEST_F(RulesetMatcherTest, ShouldBlockRequest) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  EXPECT_TRUE(matcher->ShouldBlockRequest(
      GURL("http://google.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true));
  EXPECT_FALSE(matcher->ShouldBlockRequest(
      GURL("http://yahoo.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true));
}

// Tests a simple redirect rule.
TEST_F(RulesetMatcherTest, ShouldRedirectRequest) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://yahoo.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL redirect_url;
  EXPECT_TRUE(matcher->ShouldRedirectRequest(
      GURL("http://google.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true, &redirect_url));
  EXPECT_EQ(GURL("http://yahoo.com"), redirect_url);

  EXPECT_FALSE(matcher->ShouldRedirectRequest(
      GURL("http://yahoo.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true, &redirect_url));
}

// Tests that a modified ruleset file fails verification.
TEST_F(RulesetMatcherTest, FailedVerification) {
  RulesetSource source = CreateTemporarySource();
  std::unique_ptr<RulesetMatcher> matcher;
  int expected_checksum;
  ASSERT_NO_FATAL_FAILURE(
      CreateVerifiedMatcher({}, source, &matcher, &expected_checksum));

  // Persist invalid data to the ruleset file and ensure that a version mismatch
  // occurs.
  std::string data = "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorVersionMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source.indexed_path(),
                                                  expected_checksum, &matcher));

  // Now, persist invalid data to the ruleset file, while maintaining the
  // correct version header. Ensure that it fails verification due to checksum
  // mismatch.
  data = GetVersionHeaderForTesting() + "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorChecksumMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source.indexed_path(),
                                                  expected_checksum, &matcher));
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
