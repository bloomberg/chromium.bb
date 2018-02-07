// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/tools/filter_tool.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

// See if two strings contain the same lines (regardless of line ordering).
bool StringsHaveSameLines(const std::string& a, const std::string& b) {
  std::istringstream stream_a(a), stream_b(b);
  std::vector<std::string> lines_a, lines_b;
  std::string line;

  while (std::getline(stream_a, line))
    lines_a.push_back(line);

  while (std::getline(stream_b, line))
    lines_b.push_back(line);

  std::sort(lines_a.begin(), lines_a.end());
  std::sort(lines_b.begin(), lines_b.end());
  return lines_a == lines_b;
}

class FilterToolTest : public ::testing::Test {
 public:
  FilterToolTest() {}

 protected:
  void SetUp() override {
    CreateRuleset();
    filter_tool_ = std::make_unique<FilterTool>(ruleset_, &out_stream_);
  }

  void CreateRuleset() {
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateSuffixRule("disallowed1.png"));
    rules.push_back(testing::CreateSuffixRule("disallowed2.png"));
    rules.push_back(testing::CreateSuffixRule("disallowed3.png"));
    rules.push_back(
        testing::CreateWhitelistSuffixRule("whitelist/disallowed1.png"));
    rules.push_back(
        testing::CreateWhitelistSuffixRule("whitelist/disallowed2.png"));

    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    ruleset_ = new MemoryMappedRuleset(
        testing::TestRuleset::Open(test_ruleset_pair_.indexed));
  }

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  std::ostringstream out_stream_;
  std::unique_ptr<FilterTool> filter_tool_;

  DISALLOW_COPY_AND_ASSIGN(FilterToolTest);
};

TEST_F(FilterToolTest, MatchBlacklist) {
  filter_tool_->Match("http://example.com",
                      "http://example.com/disallowed1.png", "image");

  std::string expected =
      "BLOCKED disallowed1.png| http://example.com "
      "http://example.com/disallowed1.png "
      "image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchWhitelist) {
  filter_tool_->Match("http://example.com",
                      "http://example.com/whitelist/disallowed1.png", "image");
  std::string expected =
      "ALLOWED @@whitelist/disallowed1.png| http://example.com "
      "http://example.com/whitelist/disallowed1.png "
      "image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, NoMatch) {
  filter_tool_->Match("http://example.com", "http://example.com/noproblem.png",
                      "image");

  std::string expected =
      "ALLOWED http://example.com http://example.com/noproblem.png image\n";
  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchBatch) {
  std::stringstream batch_queries;
  batch_queries
      << "http://example.com http://example.com/disallowed1.png image\n"
         "http://example.com http://example.com/disallowed2.png image\n"
         "http://example.com http://example.com/whitelist/disallowed2.png "
         "image\n";

  filter_tool_->MatchBatch(&batch_queries);

  std::string expected =
      "BLOCKED disallowed1.png| http://example.com "
      "http://example.com/disallowed1.png image\n"
      "BLOCKED disallowed2.png| http://example.com "
      "http://example.com/disallowed2.png image\n"
      "ALLOWED @@whitelist/disallowed2.png| http://example.com "
      "http://example.com/whitelist/disallowed2.png image\n";

  EXPECT_EQ(expected, out_stream_.str());
}

TEST_F(FilterToolTest, MatchRules) {
  std::stringstream batch_queries;
  batch_queries
      << "http://example.com http://example.com/disallowed1.png image\n"
         "http://example.com http://example.com/disallowed1.png image\n"
         "http://example.com http://example.com/disallowed2.png image\n"
         "http://example.com http://example.com/whitelist/disallowed2.png "
         "image\n";

  filter_tool_->MatchRules(&batch_queries, 1);

  std::string result = out_stream_.str();

  std::string expected =
      "disallowed1.png|\n"
      "disallowed2.png|\n"
      "@@whitelist/disallowed2.png|\n";

  EXPECT_TRUE(StringsHaveSameLines(expected, out_stream_.str()));
}

TEST_F(FilterToolTest, MatchRulesMinCount) {
  std::stringstream batch_queries;
  batch_queries
      << "http://example.com http://example.com/disallowed1.png image\n"
         "http://example.com http://example.com/disallowed1.png image\n"
         "http://example.com http://example.com/disallowed2.png image\n"
         "http://example.com http://example.com/whitelist/disallowed2.png "
         "image\n"
         "http://example.com http://example.com/whitelist/disallowed2.png "
         "image\n"
         "http://example.com http://example.com/whitelist/disallowed2.png "
         "image\n";

  filter_tool_->MatchRules(&batch_queries, 2);

  std::string expected =
      "disallowed1.png|\n"
      "@@whitelist/disallowed2.png|\n";

  EXPECT_TRUE(StringsHaveSameLines(expected, out_stream_.str()));
}

}  // namespace

}  // namespace subresource_filter
