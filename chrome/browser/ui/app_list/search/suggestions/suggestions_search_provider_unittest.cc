// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/suggestions/suggestions_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/search_result.h"

using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsStore;

namespace app_list {
namespace test {

namespace {

void AddSuggestion(SuggestionsProfile* suggestions,
                   const char* title,
                   const char* url) {
  ChromeSuggestion* suggestion = suggestions->add_suggestions();
  suggestion->set_url(url);
  suggestion->set_title(title);
  // TODO(mathp): Instead of using Now() in the code under test, inject a
  // reference time for testing. crbug.com/440252.
  suggestion->set_expiry_ts(base::Time::Max().ToInternalValue());
}

bool MostRelevant(const SearchResult* result1, const SearchResult* result2) {
  return result1->relevance() > result2->relevance();
}

}  // namespace

class SuggestionsSearchProviderTest : public AppListTestBase {
 public:
  SuggestionsSearchProviderTest() {}
  ~SuggestionsSearchProviderTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    profile_ = MakeSignedInTestingProfile();
    suggestions_search_.reset(
        new SuggestionsSearchProvider(profile_.get(), NULL));

    sync_preferences::TestingPrefServiceSyncable* pref_service =
        profile_->GetTestingPrefService();
    suggestions_store_.reset(new SuggestionsStore(pref_service));
  }

  void TearDown() override {
    // Making sure suggestions are cleared between tests.
    suggestions_store_->ClearSuggestions();
    profile_.reset();
  }

  std::string RunQuery(const std::string& query) {
    suggestions_search_->Start(false, base::UTF8ToUTF16(query));

    // Sort results from most to least relevant.
    std::vector<SearchResult*> sorted_results;
    sorted_results.reserve(suggestions_search_->results().size());
    for (const auto& result : suggestions_search_->results())
      sorted_results.emplace_back(result.get());
    std::sort(sorted_results.begin(), sorted_results.end(), &MostRelevant);

    std::string result_str;
    for (size_t i = 0; i < sorted_results.size(); ++i) {
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(sorted_results[i]->title());
    }
    return result_str;
  }

 protected:
  // Used to store fake suggestions data.
  std::unique_ptr<SuggestionsStore> suggestions_store_;

 private:
  // Under test.
  std::unique_ptr<SuggestionsSearchProvider> suggestions_search_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSearchProviderTest);
};

TEST_F(SuggestionsSearchProviderTest, Basic) {
  EXPECT_EQ("", RunQuery("anything other than empty string"));

  // Empty query with no suggestions in cache returns nothing.
  EXPECT_EQ("", RunQuery(""));
}

TEST_F(SuggestionsSearchProviderTest, OneSuggestion) {
  SuggestionsProfile profile;
  AddSuggestion(&profile, "title 1", "http://foo.com");
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(profile));

  // Empty query with 1 suggestion in cache returns it.
  EXPECT_EQ("title 1", RunQuery(""));

  // Non-empty query returns nothing.
  EXPECT_EQ("", RunQuery("t"));
}

TEST_F(SuggestionsSearchProviderTest, ManySuggestions) {
  SuggestionsProfile profile;
  AddSuggestion(&profile, "foo", "http://foo.com");
  AddSuggestion(&profile, "bar", "http://bar.com");
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(profile));

  // Empty query done, while there are 2 suggestions in the cache, returns
  // those suggestions, with "foo" being more relevant since it came first in
  // the suggestions profile.
  EXPECT_EQ("foo,bar", RunQuery(""));

  // Non-empty query returns nothing.
  EXPECT_EQ("", RunQuery("t"));
}

}  // namespace test
}  // namespace app_list
