// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/blacklist_store.h"

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/statistics_delta_reader.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

TEST(BlacklistStoreTest, BasicInteractions) {
  TestingPrefServiceSyncable prefs;
  BlacklistStore::RegisterProfilePrefs(prefs.registry());
  BlacklistStore blacklist_store(&prefs);

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

TEST(BlacklistStoreTest, BlacklistTwiceSuceeds) {
  TestingPrefServiceSyncable prefs;
  BlacklistStore::RegisterProfilePrefs(prefs.registry());
  BlacklistStore blacklist_store(&prefs);
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
}

TEST(BlacklistStoreTest, RemoveUnknownUrlSucceeds) {
  TestingPrefServiceSyncable prefs;
  BlacklistStore::RegisterProfilePrefs(prefs.registry());
  BlacklistStore blacklist_store(&prefs);
  EXPECT_TRUE(blacklist_store.RemoveUrl(GURL(kTestUrlA)));
}

TEST(BlacklistStoreTest, GetFirstUrlFromBlacklist) {
  TestingPrefServiceSyncable prefs;
  BlacklistStore::RegisterProfilePrefs(prefs.registry());
  BlacklistStore blacklist_store(&prefs);

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

TEST(BlacklistStoreLogTest, LogsBlacklistSize) {
  content::TestBrowserThreadBundle bundle;
  base::StatisticsDeltaReader statistics_delta_reader;

  // Create a first store - blacklist is empty at this point.
  TestingPrefServiceSyncable prefs;
  BlacklistStore::RegisterProfilePrefs(prefs.registry());
  scoped_ptr<BlacklistStore> blacklist_store(new BlacklistStore(&prefs));
  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          "Suggestions.LocalBlacklistSize"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));

  // Add some content to the blacklist.
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlB)));

  // Create a new BlacklistStore and verify the counts.
  blacklist_store.reset(new BlacklistStore(&prefs));
  samples = statistics_delta_reader.GetHistogramSamplesSinceCreation(
      "Suggestions.LocalBlacklistSize");
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));
  EXPECT_EQ(1, samples->GetCount(2));
}

}  // namespace suggestions
