// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/blacklist_store.h"

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/statistics_delta_reader.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_prefs::TestingPrefServiceSyncable;

namespace suggestions {

namespace {

const char kTestUrlA[] = "http://aaa.com/";
const char kTestUrlB[] = "http://bbb.com/";
const char kTestUrlC[] = "http://ccc.com/";
const char kTestUrlD[] = "http://ddd.com/";

SuggestionsProfile CreateSuggestions(std::set<std::string> urls) {
  SuggestionsProfile suggestions;
  for (std::set<std::string>::iterator it = urls.begin(); it != urls.end();
       ++it) {
    ChromeSuggestion* suggestion = suggestions.add_suggestions();
    suggestion->set_url(*it);
  }
  return suggestions;
}

void ValidateSuggestions(const SuggestionsProfile& expected,
                         const SuggestionsProfile& actual) {
  ASSERT_EQ(expected.suggestions_size(), actual.suggestions_size());
  for (int i = 0; i < expected.suggestions_size(); ++i) {
    EXPECT_EQ(expected.suggestions(i).url(), actual.suggestions(i).url());
    EXPECT_EQ(expected.suggestions(i).title(), actual.suggestions(i).title());
    EXPECT_EQ(expected.suggestions(i).favicon_url(),
              actual.suggestions(i).favicon_url());
    EXPECT_EQ(expected.suggestions(i).thumbnail(),
              actual.suggestions(i).thumbnail());
  }
}

}  // namespace

class BlacklistStoreTest : public testing::Test {
 public:
  BlacklistStoreTest()
    : pref_service_(new user_prefs::TestingPrefServiceSyncable) {}

  virtual void SetUp() OVERRIDE {
    BlacklistStore::RegisterProfilePrefs(pref_service()->registry());
  }

  user_prefs::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  scoped_ptr<user_prefs::TestingPrefServiceSyncable> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistStoreTest);
};

TEST_F(BlacklistStoreTest, BasicInteractions) {
  BlacklistStore blacklist_store(pref_service());

  // Create suggestions with A, B and C. C and D will be added to the blacklist.
  std::set<std::string> suggested_urls;
  suggested_urls.insert(kTestUrlA);
  suggested_urls.insert(kTestUrlB);
  const SuggestionsProfile suggestions_filtered =
      CreateSuggestions(suggested_urls);
  suggested_urls.insert(kTestUrlC);
  const SuggestionsProfile original_suggestions =
      CreateSuggestions(suggested_urls);
  SuggestionsProfile suggestions;

  // Filter with an empty blacklist.
  suggestions.CopyFrom(original_suggestions);
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);

  // Add C and D to the blacklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlC)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlD)));
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(suggestions_filtered, suggestions);

  // Remove C from the blacklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blacklist_store.RemoveUrl(GURL(kTestUrlC)));
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);
}

TEST_F(BlacklistStoreTest, BlacklistTwiceSuceeds) {
  BlacklistStore blacklist_store(pref_service());
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
}

TEST_F(BlacklistStoreTest, RemoveUnknownUrlSucceeds) {
  BlacklistStore blacklist_store(pref_service());
  EXPECT_TRUE(blacklist_store.RemoveUrl(GURL(kTestUrlA)));
}

TEST_F(BlacklistStoreTest, GetFirstUrlFromBlacklist) {
  BlacklistStore blacklist_store(pref_service());

  // Expect GetFirstUrlFromBlacklist fails when blacklist empty.
  GURL retrieved;
  EXPECT_FALSE(blacklist_store.GetFirstUrlFromBlacklist(&retrieved));

  // Blacklist A and B.
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlB)));

  // Expect to retrieve A or B.
  EXPECT_TRUE(blacklist_store.GetFirstUrlFromBlacklist(&retrieved));
  std::string retrieved_string = retrieved.spec();
  EXPECT_TRUE(retrieved_string == std::string(kTestUrlA) ||
              retrieved_string == std::string(kTestUrlB));
}

TEST_F(BlacklistStoreTest, LogsBlacklistSize) {
  base::StatisticsDeltaReader statistics_delta_reader;

  // Create a first store - blacklist is empty at this point.
  scoped_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(pref_service()));
  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          "Suggestions.LocalBlacklistSize"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));

  // Add some content to the blacklist.
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlB)));

  // Create a new BlacklistStore and verify the counts.
  blacklist_store.reset(new BlacklistStore(pref_service()));
  samples = statistics_delta_reader.GetHistogramSamplesSinceCreation(
      "Suggestions.LocalBlacklistSize");
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(2));
}

}  // namespace suggestions
