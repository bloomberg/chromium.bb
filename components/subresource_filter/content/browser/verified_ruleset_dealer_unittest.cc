// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/test/test_simple_task_runner.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

// TODO(pkalinnikov): Consider putting this to a test_support for this test file
// and SubresourceFilterRulesetDealerTest.
class TestRulesets {
 public:
  TestRulesets() = default;

  void CreateRulesets(bool many_rules = false) {
    if (many_rules) {
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithManySuffixes(
              kTestRulesetSuffix1, kNumberOfRulesInBigRuleset,
              &test_ruleset_pair_1_));
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithManySuffixes(
              kTestRulesetSuffix2, kNumberOfRulesInBigRuleset,
              &test_ruleset_pair_2_));
    } else {
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
              kTestRulesetSuffix1, &test_ruleset_pair_1_));
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
              kTestRulesetSuffix2, &test_ruleset_pair_2_));
    }
  }

  const testing::TestRuleset& indexed_1() const {
    return test_ruleset_pair_1_.indexed;
  }

  const testing::TestRuleset& indexed_2() const {
    return test_ruleset_pair_2_.indexed;
  }

 private:
  static constexpr const char kTestRulesetSuffix1[] = "foo";
  static constexpr const char kTestRulesetSuffix2[] = "bar";
  static constexpr int kNumberOfRulesInBigRuleset = 500;

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_1_;
  testing::TestRulesetPair test_ruleset_pair_2_;

  DISALLOW_COPY_AND_ASSIGN(TestRulesets);
};

constexpr const char TestRulesets::kTestRulesetSuffix1[];
constexpr const char TestRulesets::kTestRulesetSuffix2[];
constexpr int TestRulesets::kNumberOfRulesInBigRuleset;

std::vector<uint8_t> ReadRulesetContents(const MemoryMappedRuleset* ruleset) {
  return std::vector<uint8_t>(ruleset->data(),
                              ruleset->data() + ruleset->length());
}

}  // namespace

// Tests for VerifiedRulesetDealer. --------------------------------------------
//
// Note that VerifiedRulesetDealer uses RulesetDealer very directly to provide
// MemoryMappedRulesets. Many aspects of its work, e.g., lifetime of a
// MemoryMappedRuleset, its lazy creation, etc., are covered with tests to
// RulesetDealer, therefore these aspects are not tested here.

class SubresourceFilterVerifiedRulesetDealerTest : public ::testing::Test {
 public:
  SubresourceFilterVerifiedRulesetDealerTest() = default;

 protected:
  void SetUp() override {
    rulesets_.CreateRulesets(true /* many_rules */);
    ruleset_dealer_.reset(new VerifiedRulesetDealer);
  }

  const TestRulesets& rulesets() const { return rulesets_; }
  VerifiedRulesetDealer* ruleset_dealer() { return ruleset_dealer_.get(); }

  bool has_cached_ruleset() const {
    return ruleset_dealer_->has_cached_ruleset();
  }

 private:
  TestRulesets rulesets_;
  std::unique_ptr<VerifiedRulesetDealer> ruleset_dealer_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterVerifiedRulesetDealerTest);
};

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       RulesetIsMemoryMappedAndVerifiedLazily) {
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::NOT_VERIFIED,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_TRUE(ref_to_ruleset);
  EXPECT_TRUE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::INTACT, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       CorruptedRulesetIsNeitherProvidedNorCached) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 123);

  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::NOT_VERIFIED,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::CORRUPT, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       TruncatingFileMakesRulesetInvalid) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 4096);
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::CORRUPT, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       FillingRangeMakesRulesetInvalid) {
  testing::TestRuleset::CorruptByFilling(rulesets().indexed_1(),
                                         2501 /* from */, 4000 /* to */,
                                         255 /* fill_with */);
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::CORRUPT, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       RulesetIsVerifiedAfterUpdate) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 123);

  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::NOT_VERIFIED,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::CORRUPT, ruleset_dealer()->status());

  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_2()));
  EXPECT_EQ(RulesetVerificationStatus::NOT_VERIFIED,
            ruleset_dealer()->status());
  ref_to_ruleset = ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_TRUE(ref_to_ruleset);
  EXPECT_TRUE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::INTACT, ruleset_dealer()->status());
}

// Tests for VerifiedRulesetDealer::Handle. ------------------------------------

namespace {

class TestVerifiedRulesetDealerClient {
 public:
  TestVerifiedRulesetDealerClient() = default;

  base::Callback<void(VerifiedRulesetDealer*)> GetCallback() {
    return base::Bind(&TestVerifiedRulesetDealerClient::Callback,
                      base::Unretained(this));
  }

  void ExpectRulesetState(bool expected_availability,
                          RulesetVerificationStatus expected_status =
                              RulesetVerificationStatus::NOT_VERIFIED) const {
    ASSERT_EQ(1, invocation_counter_);
    EXPECT_EQ(expected_availability, is_ruleset_file_available_);
    EXPECT_FALSE(has_cached_ruleset_);
    EXPECT_EQ(expected_status, status_);
  }

  void ExpectRulesetContents(
      const std::vector<uint8_t> expected_contents) const {
    ExpectRulesetState(true, RulesetVerificationStatus::INTACT);
    EXPECT_TRUE(ruleset_is_created_);
    EXPECT_EQ(expected_contents, contents_);
  }

 private:
  void Callback(VerifiedRulesetDealer* dealer) {
    ++invocation_counter_;
    ASSERT_TRUE(dealer);

    is_ruleset_file_available_ = dealer->IsRulesetFileAvailable();
    has_cached_ruleset_ = dealer->has_cached_ruleset();
    status_ = dealer->status();

    auto ruleset = dealer->GetRuleset();
    ruleset_is_created_ = !!ruleset;
    if (ruleset_is_created_)
      contents_ = ReadRulesetContents(ruleset.get());
  }

  bool is_ruleset_file_available_ = false;
  bool has_cached_ruleset_ = false;
  RulesetVerificationStatus status_ = RulesetVerificationStatus::NOT_VERIFIED;

  bool ruleset_is_created_ = false;
  std::vector<uint8_t> contents_;

  int invocation_counter_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestVerifiedRulesetDealerClient);
};

}  // namespace

class SubresourceFilterVerifiedRulesetDealerHandleTest
    : public ::testing::Test {
 public:
  SubresourceFilterVerifiedRulesetDealerHandleTest() = default;

 protected:
  void SetUp() override {
    rulesets_.CreateRulesets(false /* many_rules */);
    task_runner_ = new base::TestSimpleTaskRunner;
  }

  const TestRulesets& rulesets() const { return rulesets_; }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  TestRulesets rulesets_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterVerifiedRulesetDealerHandleTest);
};

TEST_F(SubresourceFilterVerifiedRulesetDealerHandleTest,
       RulesetIsMappedLazily) {
  TestVerifiedRulesetDealerClient before_set_ruleset;
  TestVerifiedRulesetDealerClient after_set_ruleset;
  TestVerifiedRulesetDealerClient after_warm_up;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle(
      new VerifiedRulesetDealer::Handle(task_runner()));
  dealer_handle->GetDealerAsync(before_set_ruleset.GetCallback());
  dealer_handle->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  dealer_handle->GetDealerAsync(after_set_ruleset.GetCallback());
  dealer_handle->GetDealerAsync(after_warm_up.GetCallback());
  dealer_handle.reset(nullptr);
  task_runner()->RunUntilIdle();

  before_set_ruleset.ExpectRulesetState(false);
  after_set_ruleset.ExpectRulesetState(true);
  after_warm_up.ExpectRulesetContents(rulesets().indexed_1().contents);
}

TEST_F(SubresourceFilterVerifiedRulesetDealerHandleTest, RulesetFileIsUpdated) {
  TestVerifiedRulesetDealerClient after_set_ruleset_1;
  TestVerifiedRulesetDealerClient read_ruleset_1;
  TestVerifiedRulesetDealerClient after_set_ruleset_2;
  TestVerifiedRulesetDealerClient read_ruleset_2;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle(
      new VerifiedRulesetDealer::Handle(task_runner()));

  dealer_handle->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  dealer_handle->GetDealerAsync(after_set_ruleset_1.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_1.GetCallback());

  dealer_handle->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_2()));
  dealer_handle->GetDealerAsync(after_set_ruleset_2.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_2.GetCallback());

  dealer_handle.reset(nullptr);
  task_runner()->RunUntilIdle();

  after_set_ruleset_1.ExpectRulesetState(true);
  read_ruleset_1.ExpectRulesetContents(rulesets().indexed_1().contents);
  after_set_ruleset_2.ExpectRulesetState(true);
  read_ruleset_2.ExpectRulesetContents(rulesets().indexed_2().contents);
}

}  // namespace subresource_filter
