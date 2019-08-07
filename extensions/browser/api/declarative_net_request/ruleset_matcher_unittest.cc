// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

class RulesetMatcherTest : public ::testing::Test {
 public:
  RulesetMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcherTest);
};

// Tests a simple blocking rule.
TEST_F(RulesetMatcherTest, BlockingRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_block_request = [&matcher](const RequestParams& params) {
    return !matcher->HasMatchingAllowRule(params) &&
           matcher->HasMatchingBlockRule(params);
  };

  GURL google_url("http://google.com");
  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_TRUE(should_block_request(params));

  GURL yahoo_url("http://yahoo.com");
  params.url = &yahoo_url;
  EXPECT_FALSE(should_block_request(params));
}

// Tests a simple redirect rule.
TEST_F(RulesetMatcherTest, RedirectRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://yahoo.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_redirect_request = [&matcher](const RequestParams& params,
                                            GURL* redirect_url) {
    return matcher->GetRedirectRule(params, redirect_url) != nullptr;
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  GURL redirect_url;
  EXPECT_TRUE(should_redirect_request(params, &redirect_url));
  EXPECT_EQ(yahoo_url, redirect_url);

  params.url = &yahoo_url;
  EXPECT_FALSE(should_redirect_request(params, &redirect_url));
}

// Test that a URL cannot redirect to itself, as filed in crbug.com/954646.
TEST_F(RulesetMatcherTest, PreventSelfRedirect) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("go*");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL url("http://google.com");
  RequestParams params;
  params.url = &url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  GURL redirect_url;
  EXPECT_FALSE(matcher->GetRedirectRule(params, &redirect_url));
}

// Tests a simple upgrade scheme rule.
TEST_F(RulesetMatcherTest, UpgradeRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("upgradeScheme");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_upgrade_request = [&matcher](const RequestParams& params) {
    return matcher->GetUpgradeRule(params) != nullptr;
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");
  GURL non_upgradeable_url("https://google.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_TRUE(should_upgrade_request(params));

  params.url = &yahoo_url;
  EXPECT_FALSE(should_upgrade_request(params));

  params.url = &non_upgradeable_url;
  EXPECT_FALSE(should_upgrade_request(params));
}

// Tests that a modified ruleset file fails verification.
TEST_F(RulesetMatcherTest, FailedVerification) {
  RulesetSource source = CreateTemporarySource();
  std::unique_ptr<RulesetMatcher> matcher;
  int expected_checksum;
  ASSERT_TRUE(CreateVerifiedMatcher({}, source, &matcher, &expected_checksum));

  // Persist invalid data to the ruleset file and ensure that a version mismatch
  // occurs.
  std::string data = "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorVersionMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source, expected_checksum,
                                                  &matcher));

  // Now, persist invalid data to the ruleset file, while maintaining the
  // correct version header. Ensure that it fails verification due to checksum
  // mismatch.
  data = GetVersionHeaderForTesting() + "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(source.indexed_path(), data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorChecksumMismatch,
            RulesetMatcher::CreateVerifiedMatcher(source, expected_checksum,
                                                  &matcher));
}

// Tests IsExtraHeadersMatcher and GetRemoveHeadersMask.
TEST_F(RulesetMatcherTest, RemoveHeaders) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_FALSE(matcher->IsExtraHeadersMatcher());

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  rule.action->type = std::string("removeHeaders");
  rule.action->remove_headers_list =
      std::vector<std::string>({"referer", "setCookie"});
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
  EXPECT_EQ(kRemoveHeadersMask_Referer | kRemoveHeadersMask_SetCookie,
            matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  GURL google_url("http://google.com");
  params.url = &google_url;
  EXPECT_EQ(0u, matcher->GetRemoveHeadersMask(params, 0u /* current_mask */));

  // The current mask is ignored while matching and returned as part of the
  // result.
  uint8_t current_mask =
      kRemoveHeadersMask_Referer | kRemoveHeadersMask_SetCookie;
  EXPECT_EQ(current_mask, matcher->GetRemoveHeadersMask(params, current_mask));
}

// Tests a rule to redirect to an extension path.
TEST_F(RulesetMatcherTest, RedirectToExtensionPath) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->extension_path = "/path/newfile.js?query#fragment";

  std::unique_ptr<RulesetMatcher> matcher;
  const size_t kId = 1;
  const size_t kPriority = 1;
  const size_t kRuleCountLimit = 10;
  const ExtensionId extension_id = "extension_id";
  ASSERT_TRUE(CreateVerifiedMatcher(
      {rule},
      CreateTemporarySource(kId, kPriority, kRuleCountLimit, extension_id),
      &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  GURL redirect_url;
  EXPECT_TRUE(matcher->GetRedirectRule(params, &redirect_url));
  GURL expected_redirect_url(
      "chrome-extension://extension_id/path/newfile.js?query#fragment");
  EXPECT_EQ(expected_redirect_url, redirect_url);
}

// Tests a rule to redirect to a static url.
TEST_F(RulesetMatcherTest, RedirectToStaticUrl) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->url = "https://google.com";

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  GURL redirect_url;
  GURL expected_redirect_url("https://google.com");
  EXPECT_TRUE(matcher->GetRedirectRule(params, &redirect_url));
  EXPECT_EQ(expected_redirect_url, redirect_url);
}

// Tests url transformation rules.
TEST_F(RulesetMatcherTest, UrlTransform) {
  struct TestCase {
    std::string url;
    // Valid if a redirect is expected.
    base::Optional<std::string> expected_redirect_url;
  };

  std::vector<TestCase> cases;
  std::vector<TestRule> rules;

  auto create_transform_rule = [](size_t id, const std::string& filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = filter;
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->transform.emplace();
    return rule;
  };

  TestRule rule = create_transform_rule(1, "||1.com");
  rule.action->redirect->transform->scheme = "https";
  rules.push_back(rule);
  cases.push_back({"http://1.com/path?query", "https://1.com/path?query"});

  rule = create_transform_rule(2, "||2.com");
  rule.action->redirect->transform->scheme = "ftp";
  rule.action->redirect->transform->host = "ftp.host.com";
  rule.action->redirect->transform->port = "70";
  rules.push_back(rule);
  cases.push_back(
      {"http://2.com:100/path?query", "ftp://ftp.host.com:70/path?query"});

  rule = create_transform_rule(3, "||3.com");
  rule.action->redirect->transform->port = "";
  rule.action->redirect->transform->path = "";
  rule.action->redirect->transform->query = "?new_query";
  rule.action->redirect->transform->fragment = "#fragment";
  rules.push_back(rule);
  // The path separator '/' is output even when cleared, except when there is no
  // authority, query and fragment.
  cases.push_back(
      {"http://3.com:100/path?query", "http://3.com/?new_query#fragment"});

  rule = create_transform_rule(4, "||4.com");
  rule.action->redirect->transform->scheme = "http";
  rule.action->redirect->transform->path = " ";
  rules.push_back(rule);
  cases.push_back({"http://4.com/xyz", "http://4.com/%20"});

  rule = create_transform_rule(5, "||5.com");
  rule.action->redirect->transform->path = "/";
  rule.action->redirect->transform->query = "";
  rule.action->redirect->transform->fragment = "#";
  rules.push_back(rule);
  cases.push_back(
      {"http://5.com:100/path?query#fragment", "http://5.com:100/#"});

  rule = create_transform_rule(6, "||6.com");
  rule.action->redirect->transform->path = "/path?query";
  rules.push_back(rule);
  // The "?" in path is url encoded since it's not part of the query.
  cases.push_back({"http://6.com/xyz?q1", "http://6.com/path%3Fquery?q1"});

  rule = create_transform_rule(7, "||7.com");
  rule.action->redirect->transform->username = "new_user";
  rule.action->redirect->transform->password = "new_pass";
  rules.push_back(rule);
  cases.push_back(
      {"http://user@7.com/xyz", "http://new_user:new_pass@7.com/xyz"});

  auto make_query = [](const std::string& key, const std::string& value) {
    TestRuleQueryKeyValue query;
    query.key = key;
    query.value = value;
    return query;
  };

  rule = create_transform_rule(8, "||8.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>(
          {make_query("a1", "#"), make_query("a2", ""),
           make_query("a1", "new2"), make_query("a1", "new3")});
  rules.push_back(rule);
  cases.push_back(
      {"http://8.com/"
       "path?r1&r1=val1&a1=val1&r2=val&x3=val&a1=val2&a2=val&r1=val2",
       "http://8.com/path?a1=%23&x3=val&a1=new2&a2=&a1=new3"});
  cases.push_back({"http://8.com/path?query",
                   "http://8.com/path?query=&a1=%23&a2=&a1=new2&a1=new3"});

  rule = create_transform_rule(9, "||9.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rules.push_back(rule);
  // No redirect is performed since the url won't change.
  cases.push_back({"http://9.com/path?query#fragment", base::nullopt});

  rule = create_transform_rule(10, "||10.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"q1"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>({make_query("q1", "new")});
  rules.push_back(rule);
  cases.push_back(
      {"https://10.com/path?q1=1&q1=2&q1=3", "https://10.com/path?q1=new"});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing url %s", test_case.url.c_str()));

    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid()) << test_case.url;
    RequestParams params;
    params.url = &url;

    GURL redirect_url;
    if (!test_case.expected_redirect_url) {
      EXPECT_FALSE(matcher->GetRedirectRule(params, &redirect_url))
          << redirect_url.spec();
      continue;
    }

    ASSERT_TRUE(GURL(*test_case.expected_redirect_url).is_valid())
        << *test_case.expected_redirect_url;
    EXPECT_TRUE(matcher->GetRedirectRule(params, &redirect_url));
    EXPECT_EQ(*test_case.expected_redirect_url, redirect_url.spec());
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
