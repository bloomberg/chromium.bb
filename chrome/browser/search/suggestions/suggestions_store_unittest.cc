// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_store.h"

#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace suggestions {

namespace {

const char kTestTitle[] = "Foo site";
const char kTestUrl[] = "http://foo.com/";

SuggestionsProfile CreateTestSuggestions() {
  SuggestionsProfile suggestions;
  ChromeSuggestion* suggestion = suggestions.add_suggestions();
  suggestion->set_url(kTestUrl);
  suggestion->set_title(kTestTitle);
  return suggestions;
}

void ValidateSuggestions(const SuggestionsProfile& expected,
                         const SuggestionsProfile& actual) {
  EXPECT_EQ(expected.suggestions_size(), actual.suggestions_size());
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

TEST(SuggestionsStoreTest, LoadStoreClear) {
  TestingPrefServiceSyncable prefs;
  SuggestionsStore::RegisterProfilePrefs(prefs.registry());
  SuggestionsStore suggestions_store(&prefs);

  const SuggestionsProfile suggestions = CreateTestSuggestions();
  const SuggestionsProfile empty_suggestions;
  SuggestionsProfile recovered_suggestions;

  // Attempt to load when prefs are empty.
  EXPECT_FALSE(suggestions_store.LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(empty_suggestions, recovered_suggestions);

  // Store then reload.
  EXPECT_TRUE(suggestions_store.StoreSuggestions(suggestions));
  EXPECT_TRUE(suggestions_store.LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(suggestions, recovered_suggestions);

  // Clear.
  suggestions_store.ClearSuggestions();
  EXPECT_FALSE(suggestions_store.LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(empty_suggestions, recovered_suggestions);
}

}  // namespace suggestions
