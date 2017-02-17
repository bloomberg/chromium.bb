// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/document_subresource_filter.h"

#include "base/files/file.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

constexpr auto kDisabled = ActivationLevel::DISABLED;
constexpr auto kDryRun = ActivationLevel::DRYRUN;
constexpr auto kEnabled = ActivationLevel::ENABLED;

constexpr auto kImageType = proto::ELEMENT_TYPE_IMAGE;
constexpr auto kSubdocumentType = proto::ELEMENT_TYPE_SUBDOCUMENT;

constexpr const char kTestAlphaURL[] = "http://example.com/alpha";
constexpr const char kTestAlphaDataURI[] = "data:text/plain,alpha";
constexpr const char kTestBetaURL[] = "http://example.com/beta";

constexpr const char kTestAlphaURLPathSuffix[] = "alpha";

}  // namespace

// Tests for DocumentSubresourceFilter class. ----------------------------------

class DocumentSubresourceFilterTest : public ::testing::Test {
 public:
  DocumentSubresourceFilterTest() {}

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestAlphaURLPathSuffix));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(base::StringPiece suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_ = new MemoryMappedRuleset(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilterTest);
};

TEST_F(DocumentSubresourceFilterTest, DryRun) {
  ActivationState activation_state(kDryRun);
  activation_state.measure_performance = true;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset());

  EXPECT_EQ(LoadPolicy::WOULD_DISALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaURL), kImageType));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaDataURI), kImageType));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestBetaURL), kImageType));
  EXPECT_EQ(LoadPolicy::WOULD_DISALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaURL), kSubdocumentType));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestBetaURL), kSubdocumentType));

  const auto& statistics = filter.statistics();
  EXPECT_EQ(5, statistics.num_loads_total);
  EXPECT_EQ(4, statistics.num_loads_evaluated);
  EXPECT_EQ(2, statistics.num_loads_matching_rules);
  EXPECT_EQ(0, statistics.num_loads_disallowed);
}

TEST_F(DocumentSubresourceFilterTest, Enabled) {
  auto test_impl = [this](bool measure_performance) {
    ActivationState activation_state(kEnabled);
    activation_state.measure_performance = measure_performance;
    DocumentSubresourceFilter filter(url::Origin(), activation_state,
                                     ruleset());

    EXPECT_EQ(LoadPolicy::DISALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaURL), kImageType));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaDataURI), kImageType));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestBetaURL), kImageType));
    EXPECT_EQ(LoadPolicy::DISALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaURL), kSubdocumentType));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestBetaURL), kSubdocumentType));

    const auto& statistics = filter.statistics();
    EXPECT_EQ(5, statistics.num_loads_total);
    EXPECT_EQ(4, statistics.num_loads_evaluated);
    EXPECT_EQ(2, statistics.num_loads_matching_rules);
    EXPECT_EQ(2, statistics.num_loads_disallowed);

    if (!measure_performance) {
      EXPECT_TRUE(statistics.evaluation_total_cpu_duration.is_zero());
      EXPECT_TRUE(statistics.evaluation_total_wall_duration.is_zero());
    }
    // Otherwise, don't expect |total_duration| to be non-zero, although it
    // practically is (when timer is supported).
  };

  test_impl(true /* measure_performance */);
  test_impl(false /* measure_performance */);
}

// Tests for ComputeActivationState functions. ---------------------------------

class SubresourceFilterComputeActivationStateTest : public ::testing::Test {
 public:
  SubresourceFilterComputeActivationStateTest() {}

 protected:
  void SetUp() override {
    constexpr int32_t kDocument = proto::ACTIVATION_TYPE_DOCUMENT;
    constexpr int32_t kGenericBlock = proto::ACTIVATION_TYPE_GENERICBLOCK;

    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "child1.com", kDocument, {"parent1.com", "parent2.com"}));
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "child2.com", kGenericBlock, {"parent1.com", "parent2.com"}));
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "child3.com", kDocument | kGenericBlock,
        {"parent1.com", "parent2.com"}));

    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair));
    ruleset_ = new MemoryMappedRuleset(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  static ActivationState MakeState(
      bool filtering_disabled_for_document,
      bool generic_blocking_rules_disabled = false,
      ActivationLevel activation_level = kEnabled) {
    ActivationState activation_state(activation_level);
    activation_state.filtering_disabled_for_document =
        filtering_disabled_for_document;
    activation_state.generic_blocking_rules_disabled =
        generic_blocking_rules_disabled;
    return activation_state;
  };

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterComputeActivationStateTest);
};

TEST_F(SubresourceFilterComputeActivationStateTest,
       ActivationBitsCorrectlyPropagateToChildDocument) {
  // Make sure that the |generic_blocking_rules_disabled| flag is disregarded
  // when |filtering_disabled_for_document| is true.
  ASSERT_EQ(MakeState(true, false), MakeState(true, true));

  // TODO(pkalinnikov): Find a short way to express all these tests.
  const struct {
    const char* document_url;
    const char* parent_document_origin;
    ActivationState parent_activation;
    ActivationState expected_activation_state;
  } kTestCases[] = {
      {"http://example.com", "http://example.com", MakeState(false, false),
       MakeState(false, false)},
      {"http://example.com", "http://example.com", MakeState(false, true),
       MakeState(false, true)},
      {"http://example.com", "http://example.com", MakeState(true, false),
       MakeState(true)},
      {"http://example.com", "http://example.com", MakeState(true, true),
       MakeState(true)},

      {"http://child1.com", "http://parrrrent1.com", MakeState(false, false),
       MakeState(false, false)},
      {"http://child1.com", "http://parent1.com", MakeState(false, false),
       MakeState(true, false)},
      {"http://child1.com", "http://parent2.com", MakeState(false, false),
       MakeState(true, false)},
      {"http://child1.com", "http://parent2.com", MakeState(true, false),
       MakeState(true)},
      {"http://child1.com", "http://parent2.com", MakeState(false, true),
       MakeState(true)},

      {"http://child2.com", "http://parent1.com", MakeState(false, false),
       MakeState(false, true)},
      {"http://child2.com", "http://parent1.com", MakeState(false, true),
       MakeState(false, true)},
      {"http://child2.com", "http://parent1.com", MakeState(true, false),
       MakeState(true)},
      {"http://child2.com", "http://parent1.com", MakeState(true, true),
       MakeState(true)},

      {"http://child3.com", "http://parent1.com", MakeState(false, false),
       MakeState(true)},
      {"http://child3.com", "http://parent1.com", MakeState(false, true),
       MakeState(true)},
      {"http://child3.com", "http://parent1.com", MakeState(true, false),
       MakeState(true)},
      {"http://child3.com", "http://parent1.com", MakeState(true, true),
       MakeState(true)},
  };

  for (size_t i = 0, size = arraysize(kTestCases); i != size; ++i) {
    SCOPED_TRACE(::testing::Message() << "Test number: " << i);
    const auto& test_case = kTestCases[i];

    GURL document_url(test_case.document_url);
    url::Origin parent_document_origin(GURL(test_case.parent_document_origin));
    ActivationState activation_state =
        ComputeActivationState(document_url, parent_document_origin,
                               test_case.parent_activation, ruleset());
    EXPECT_EQ(test_case.expected_activation_state, activation_state);
  }
}

TEST_F(SubresourceFilterComputeActivationStateTest,
       ActivationStateCorrectlyPropagatesDownDocumentHierarchy) {
  const struct {
    std::vector<std::string> ancestor_document_urls;
    ActivationLevel activation_level;
    ActivationState expected_activation_state;
  } kTestCases[] = {
      {{"http://example.com"}, kEnabled, MakeState(false)},
      {std::vector<std::string>(2, "http://example.com"), kEnabled,
       MakeState(false)},
      {std::vector<std::string>(4, "http://example.com"), kEnabled,
       MakeState(false)},

      {std::vector<std::string>(4, "http://example.com"), kEnabled,
       MakeState(false, false, kEnabled)},
      {std::vector<std::string>(4, "http://example.com"), kDisabled,
       MakeState(false, false, kDisabled)},
      {std::vector<std::string>(4, "http://example.com"), kDryRun,
       MakeState(false, false, kDryRun)},

      {{"http://ex.com", "http://child1.com", "http://parent1.com",
        "http://root.com"},
       kEnabled,
       MakeState(true)},

      {{"http://ex.com", "http://child1.com", "http://parent1.com",
        "http://root.com"},
       kEnabled,
       MakeState(true)},

      {{"http://ex.com", "http://child2.com", "http://parent1.com",
        "http://root.com"},
       kEnabled,
       MakeState(false, true)},

      {{"http://ex.com", "http://ex.com", "http://child3.com",
        "http://parent1.com", "http://root.com"},
       kDryRun,
       MakeState(true, false, kDryRun)},
  };

  for (size_t i = 0, size = arraysize(kTestCases); i != size; ++i) {
    const auto& test_case = kTestCases[i];
    SCOPED_TRACE(::testing::Message() << "Test number: " << i);

    std::vector<GURL> ancestor_document_urls;
    for (const auto& url_string : test_case.ancestor_document_urls)
      ancestor_document_urls.emplace_back(url_string);

    ActivationState activation_state = ComputeActivationState(
        test_case.activation_level, false, ancestor_document_urls, ruleset());
    EXPECT_EQ(test_case.expected_activation_state, activation_state);
  }
}

}  // namespace subresource_filter
