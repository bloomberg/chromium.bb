// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

class CompositeMatcherTest : public ::testing::Test {
 public:
  CompositeMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcherTest);
};

// Ensure CompositeMatcher respects priority of individual rulesets.
TEST_F(CompositeMatcherTest, RulesetPriority) {
  TestRule block_rule = CreateGenericRule();
  block_rule.condition->url_filter = std::string("google.com");
  block_rule.id = kMinValidID;

  TestRule redirect_rule_1 = CreateGenericRule();
  redirect_rule_1.condition->url_filter = std::string("example.com");
  redirect_rule_1.priority = kMinValidPriority;
  redirect_rule_1.action->type = std::string("redirect");
  redirect_rule_1.action->redirect_url = std::string("http://ruleset1.com");
  redirect_rule_1.id = kMinValidID + 1;

  // Create the first ruleset matcher.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({block_rule, redirect_rule_1},
                                    CreateTemporarySource(), &matcher_1));
  matcher_1->set_id_for_testing(kSource1ID);
  matcher_1->set_priority_for_testing(kSource1Priority);

  // Now create a second ruleset matcher.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  TestRule allow_rule = block_rule;
  allow_rule.action->type = std::string("allow");
  TestRule redirect_rule_2 = redirect_rule_1;
  redirect_rule_2.action->redirect_url = std::string("http://ruleset2.com");
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher({allow_rule, redirect_rule_2},
                                    CreateTemporarySource(), &matcher_2));
  matcher_2->set_id_for_testing(kSource2ID);
  matcher_2->set_priority_for_testing(kSource2Priority);

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher =
      std::make_unique<CompositeMatcher>(std::move(matchers));

  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // The second ruleset should get more priority.
  EXPECT_FALSE(composite_matcher->ShouldBlockRequest(google_params));

  GURL example_url = GURL("http://example.com");
  RequestParams example_params = google_params;
  example_params.url = &example_url;
  GURL redirect_url;
  EXPECT_TRUE(
      composite_matcher->ShouldRedirectRequest(example_params, &redirect_url));
  EXPECT_EQ(GURL("http://ruleset2.com"), redirect_url);

  // Now switch the priority of the two rulesets. This requires re-constructing
  // the two ruleset matchers.
  matcher_1.reset();
  matcher_2.reset();
  matchers.clear();
  ASSERT_TRUE(CreateVerifiedMatcher({block_rule, redirect_rule_1},
                                    CreateTemporarySource(), &matcher_1));
  matcher_1->set_id_for_testing(kSource1ID);
  matcher_1->set_priority_for_testing(kSource2Priority);
  ASSERT_TRUE(CreateVerifiedMatcher({allow_rule, redirect_rule_2},
                                    CreateTemporarySource(), &matcher_2));
  matcher_2->set_id_for_testing(kSource2ID);
  matcher_2->set_priority_for_testing(kSource1Priority);
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(std::move(matchers));

  // The first ruleset should get more priority.
  EXPECT_TRUE(composite_matcher->ShouldBlockRequest(google_params));
  EXPECT_TRUE(
      composite_matcher->ShouldRedirectRequest(example_params, &redirect_url));
  EXPECT_EQ(GURL("http://ruleset1.com"), redirect_url);
}

}  // namespace declarative_net_request
}  // namespace extensions
