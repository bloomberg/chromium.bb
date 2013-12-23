// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/most_visited_tiles_experiment.h"

#include <algorithm>
#include <sstream>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/instant_types.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history {

namespace {

// Constants for the most visited tile placement field trial.
// See field trial config (MostVisitedTilePlacement.json) for details.
const char kMostVisitedFieldTrialName[] = "MostVisitedTilePlacement";
const char kOneEightAGroupName[] = "OneEight_A_Flipped";
const char kOneFourAGroupName[] = "OneFour_A_Flipped";
const char kDontShowOpenURLsGroupName[] = "DontShowOpenTabs";
const char kGmailURL[] = "http://www.gmail.com/";
// Name of histogram tracking types of actions carried out by the field trial.
const char kMostVisitedExperimentHistogramName[] =
    "NewTabPage.MostVisitedTilePlacementExperiment";
// Minimum number of Most Visited suggestions required in order for the Most
// Visited Field Trial to remove a URL already open in the browser.
const size_t kMinUrlSuggestions = 8;

// The indexes of the tiles that are affected in the experiment.
enum FlippedIndexes {
  TILE_ONE = 0,
  TILE_FOUR = 3,
  TILE_EIGHT = 7
};

// Creates a DictionaryValue using |url| and appends to |list|.
void AppendURLToListValue(const std::string& url_string,
                          base::ListValue* list) {
  base::DictionaryValue* page_value = new base::DictionaryValue();
  page_value->SetString("url", url_string);
  list->Append(page_value);
}

// Creates an InstantMostVisitedItem using |url| and appends to |list|.
void AppendInstantURLToVector(const std::string& url_string,
                              std::vector<InstantMostVisitedItem>* list) {
  InstantMostVisitedItem item;
  item.url = GURL(url_string);
  list->push_back(item);
}

// Creates an MostVisitedURL using |url| and appends to |list|.
void AppendMostVisitedURLToVector(const std::string& url_string,
                                  std::vector<history::MostVisitedURL>* list) {
  history::MostVisitedURL most_visited;
  most_visited.url = GURL(url_string);
  list->push_back(most_visited);
}

void SetUpMaybeShuffle(const int& max_urls,
                       MostVisitedURLList* most_visited_urls,
                       MostVisitedURLList* test_urls) {
  // |most_visited_urls| must have > 8 MostVisitedURLs for any URLs to be
  // flipped by experiment.
  for (int i = 0; i < max_urls; ++i) {
    std::string url;
    base::SStringPrintf(&url, "http://www.test%d.com", i);
    AppendMostVisitedURLToVector(url, most_visited_urls);
    AppendMostVisitedURLToVector(url, test_urls);
  }
}

}  // namespace

class MostVisitedTilesExperimentTest : public testing::Test {
 public:
  MostVisitedTilesExperimentTest()
      : histogram_(NULL),
        field_trial_list_(new metrics::SHA1EntropyProvider("foo")) {}

  virtual ~MostVisitedTilesExperimentTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    base::StatisticsRecorder::Initialize();
    previous_metrics_count_.resize(NUM_NTP_TILE_EXPERIMENT_ACTIONS, 0);
    base::HistogramBase* histogram = GetHistogram();
    if (histogram) {
      scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
      if (samples.get()) {
        for (int state = NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL;
             state < NUM_NTP_TILE_EXPERIMENT_ACTIONS;
             ++state) {
          previous_metrics_count_[state] = samples->GetCount(state);
        }
      }
    }
  }

  void ValidateMetrics(const base::HistogramBase::Sample& value) {
    base::HistogramBase* histogram = GetHistogram();
    ASSERT_TRUE(histogram != NULL);
    scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
    if (samples.get()) {
      for (int state = NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL;
           state < NUM_NTP_TILE_EXPERIMENT_ACTIONS;
           ++state) {
        if (state == value) {
          EXPECT_EQ(previous_metrics_count_[state] + 1,
                    samples->GetCount(state));
        } else {
          EXPECT_EQ(previous_metrics_count_[state], samples->GetCount(state));
        }
      }
    }
  }

 private:
  base::HistogramBase* GetHistogram() {
    if (!histogram_) {
      histogram_ = base::StatisticsRecorder::FindHistogram(
          kMostVisitedExperimentHistogramName);
    }
    return histogram_;
  }

  // Owned by base::StatisticsRecorder
  base::HistogramBase* histogram_;
  base::FieldTrialList field_trial_list_;
  std::vector<int> previous_metrics_count_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedTilesExperimentTest);
};

// For pre-instant extended clients.
TEST_F(MostVisitedTilesExperimentTest,
       RemovePageValuesMatchingOpenTabsTooFewURLs) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kDontShowOpenURLsGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_TRUE(MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled());

  std::set<std::string> open_urls;
  open_urls.insert(kGmailURL);

  base::ListValue pages_value;
  AppendURLToListValue(kGmailURL, &pages_value);

  // Test the method when there are not enough URLs to force removal.
  MostVisitedTilesExperiment::RemovePageValuesMatchingOpenTabs(
      open_urls, &pages_value);
  base::DictionaryValue gmail_value;
  gmail_value.SetString("url", kGmailURL);
  // Ensure the open url has not been removed from |pages_value|.
  EXPECT_NE(pages_value.end(), pages_value.Find(gmail_value));

  // Ensure counts have been incremented correctly.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_DID_NOT_REMOVE_URL));
}

// For pre-instant extended clients.
TEST_F(MostVisitedTilesExperimentTest, RemovePageValuesMatchingOpenTabs) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kDontShowOpenURLsGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_TRUE(MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled());

  std::set<std::string> open_urls;
  open_urls.insert(kGmailURL);

  base::ListValue pages_value;
  AppendURLToListValue(kGmailURL, &pages_value);

  // |pages_value| must have > 8 page values for any URLs to be removed by
  // experiment.
  for (size_t i = 0; i < kMinUrlSuggestions; ++i) {
    std::string url;
    base::SStringPrintf(&url, "http://www.test%d.com", static_cast<int>(i));
    AppendURLToListValue(url, &pages_value);
  }

  // Call method with enough URLs to force removal.
  MostVisitedTilesExperiment::RemovePageValuesMatchingOpenTabs(
      open_urls, &pages_value);
  // Ensure the open url has been removed from |pages_value|.
  base::DictionaryValue gmail_value;
  gmail_value.SetString("url", kGmailURL);
  EXPECT_EQ(pages_value.end(), pages_value.Find(gmail_value));

  // Ensure counts have been incremented correctly.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL));
}

// For instant extended clients.
TEST_F(MostVisitedTilesExperimentTest, RemoveItemsMatchingOpenTabsTooFewURLs) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kDontShowOpenURLsGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_TRUE(MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled());

  std::set<std::string> open_urls;
  open_urls.insert(kGmailURL);
  std::vector<InstantMostVisitedItem> items;
  AppendInstantURLToVector(kGmailURL, &items);

  // Call the method when there are not enough URLs to force removal.
  MostVisitedTilesExperiment::RemoveItemsMatchingOpenTabs(open_urls, &items);

  // Ensure the open url has not been removed from |items|.
  for (size_t i = 0; i < items.size(); i++) {
    const std::string& item_url = items[i].url.spec();
    EXPECT_NE(0u, open_urls.count(item_url));
  }

  // Ensure counts have been incremented correctly.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_DID_NOT_REMOVE_URL));
}

// For instant extended clients.
TEST_F(MostVisitedTilesExperimentTest, RemoveItemsMatchingOpenTabs) {
  base::FieldTrialList::CreateFieldTrial(
      kMostVisitedFieldTrialName,
      kDontShowOpenURLsGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_TRUE(MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled());

  std::set<std::string> open_urls;
  open_urls.insert(kGmailURL);
  std::vector<InstantMostVisitedItem> items;
  AppendInstantURLToVector(kGmailURL, &items);

  // |items| must have > 8 InstantMostVisitedItems for any URLs to be removed by
  // experiment.
  for (size_t i = 0; i < kMinUrlSuggestions; ++i) {
    std::string url;
    base::SStringPrintf(&url, "http://www.test%d.com", static_cast<int>(i));
    AppendInstantURLToVector(url, &items);
  }

  // Call method with enough URLs to force removal.
  MostVisitedTilesExperiment::RemoveItemsMatchingOpenTabs(open_urls, &items);

  // Ensure the open URL has been removed from |items|.
  for (size_t i = 0; i < items.size(); i++) {
    const std::string& item_url = items[i].url.spec();
    EXPECT_EQ(0u, open_urls.count(item_url));
  }

  // Ensure counts have been incremented correctly.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL));
}

TEST_F(MostVisitedTilesExperimentTest, MaybeShuffleOneEight) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kOneEightAGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_EQ(kOneEightAGroupName,
            base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName));

  MostVisitedURLList most_visited_urls;
  MostVisitedURLList test_urls;
  SetUpMaybeShuffle(kMinUrlSuggestions, &most_visited_urls, &test_urls);

  history::MostVisitedTilesExperiment::MaybeShuffle(&most_visited_urls);
  // Ensure the 1st and 8th URLs have been switched.
  EXPECT_EQ(most_visited_urls[TILE_ONE].url.spec(),
            test_urls[TILE_EIGHT].url.spec());
}

TEST_F(MostVisitedTilesExperimentTest, MaybeShuffleOneEightTooFewURLs) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kOneEightAGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_EQ(kOneEightAGroupName,
            base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName));

  MostVisitedURLList most_visited_urls;
  MostVisitedURLList test_urls;
  // If |most_visited_urls| has < 8 URLs, experiment will not flip any tiles.
  SetUpMaybeShuffle(kMinUrlSuggestions - 1, &most_visited_urls, &test_urls);

  history::MostVisitedTilesExperiment::MaybeShuffle(&most_visited_urls);
  // Ensure no URLs have been switched.
  EXPECT_EQ(most_visited_urls[TILE_ONE].url.spec(),
            test_urls[TILE_ONE].url.spec());
  EXPECT_EQ(most_visited_urls[TILE_EIGHT - 1].url.spec(),
            test_urls[TILE_EIGHT - 1].url.spec());

  // Ensure counts are correct.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_8));
}

TEST_F(MostVisitedTilesExperimentTest, MaybeShuffleOneFour) {
  base::FieldTrialList::CreateFieldTrial(kMostVisitedFieldTrialName,
                                         kOneFourAGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_EQ(kOneFourAGroupName,
            base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName));

  MostVisitedURLList most_visited_urls;
  MostVisitedURLList test_urls;
  SetUpMaybeShuffle(kMinUrlSuggestions, &most_visited_urls, &test_urls);

  history::MostVisitedTilesExperiment::MaybeShuffle(&most_visited_urls);
  // Ensure the 1st and 4th URLs have been switched.
  EXPECT_EQ(most_visited_urls[TILE_ONE].url.spec(),
            test_urls[TILE_FOUR].url.spec());
}

TEST_F(MostVisitedTilesExperimentTest, MaybeShuffleOneFourTooFewURLs) {
  base::FieldTrialList::CreateFieldTrial(
      kMostVisitedFieldTrialName,
      kOneFourAGroupName);

  // Ensure the field trial is created with the correct group.
  EXPECT_EQ(kOneFourAGroupName,
            base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName));

  MostVisitedURLList most_visited_urls;
  MostVisitedURLList test_urls;
  // If |most_visited_urls| has < 4 URLs, experiment will not flip any tiles.
  SetUpMaybeShuffle(kMinUrlSuggestions - 5, &most_visited_urls, &test_urls);

  history::MostVisitedTilesExperiment::MaybeShuffle(&most_visited_urls);
  // Ensure no URLs have been switched.
  EXPECT_EQ(most_visited_urls[TILE_ONE].url.spec(),
           test_urls[TILE_ONE].url.spec());
  EXPECT_EQ(most_visited_urls[TILE_FOUR-1].url.spec(),
            test_urls[TILE_FOUR-1].url.spec());

  // Ensure counts are correct.
  EXPECT_NO_FATAL_FAILURE(
      ValidateMetrics(NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_4));
}

}  // namespace history
