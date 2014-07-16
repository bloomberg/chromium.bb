// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/common/chrome_switches.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

TEST(SearchTest, EmbeddedSearchAPIEnabled) {
  EXPECT_EQ(1ul, EmbeddedSearchPageVersion());
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableEmbeddedSearchAPI);
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
}

TEST(SearchTest, QueryExtractionEnabled) {
  // Query extraction is always enabled on mobile.
  EXPECT_TRUE(IsQueryExtractionEnabled());
}

class SearchUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::StatisticsRecorder::Initialize();
  }

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(SearchUtilTest, UseDefaultEmbeddedSearchPageVersion) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:-1 query_extraction:1"));
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ("espv=1&", InstantExtendedEnabledParam(true));
  EXPECT_EQ("espv=1&", InstantExtendedEnabledParam(false));
}

TEST_F(SearchUtilTest, ShouldPrefetchSearchResults_InstantExtendedAPIEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 prefetch_results:1"));
  EXPECT_TRUE(ShouldPrefetchSearchResults());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest, ShouldPrefetchSearchResults_InstantExtendedAPIDisabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:1 prefetch_results:1"));
  EXPECT_FALSE(ShouldPrefetchSearchResults());
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
  EXPECT_EQ(1ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest, ShouldPrefetchSearchResults_DisabledViaFieldTrials) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 prefetch_results:0"));
  EXPECT_FALSE(ShouldPrefetchSearchResults());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest, ShouldPrefetchSearchResults_EnabledViaCommandLine) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kPrefetchSearchResults);
  // Command-line enable should override Finch.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 prefetch_results:0"));
  EXPECT_TRUE(ShouldPrefetchSearchResults());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest,
       ShouldReuseInstantSearchBasePage_PrefetchResultsFlagDisabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 prefetch_results:0 reuse_instant_search_base_page:1"));
  EXPECT_FALSE(ShouldPrefetchSearchResults());
  EXPECT_FALSE(ShouldReuseInstantSearchBasePage());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest, ShouldReuseInstantSearchBasePage_EnabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 prefetch_results:1 reuse_instant_search_base_page:1"));
  EXPECT_TRUE(ShouldReuseInstantSearchBasePage());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchUtilTest, ShouldReuseInstantSearchBasePage_DisabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 prefetch_results:1 reuse_instant_search_base_page:0"));
  EXPECT_FALSE(ShouldReuseInstantSearchBasePage());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

}  // namespace

}  // namespace chrome
