// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {
namespace {

using base::Time;
using base::TimeDelta;

// Limits how frequently models are queried for ranking results.
constexpr TimeDelta kMinSecondsBetweenFetches = TimeDelta::FromSeconds(1);

// Represents each model used within the SearchResultRanker.
enum class Model { NONE, RESULTS_LIST_GROUP_RANKER };

// Returns the model relevant for predicting launches for results with the given
// |type|.
Model ModelForType(RankingItemType type) {
  switch (type) {
    case RankingItemType::kFile:
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
      return Model::RESULTS_LIST_GROUP_RANKER;
    default:
      return Model::NONE;
  }
}

}  // namespace

SearchResultRanker::SearchResultRanker(Profile* profile) {
  if (app_list_features::IsAdaptiveResultRankerEnabled()) {
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    auto* predictor = config.mutable_zero_state_frecency_predictor();
    predictor->set_target_limit(base::GetFieldTrialParamByFeatureAsInt(
        app_list_features::kEnableAdaptiveResultRanker, "target_limit", 200));
    predictor->set_decay_coeff(base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableAdaptiveResultRanker, "decay_coeff", 0.8f));
    auto* fallback = config.mutable_fallback_predictor();
    fallback->set_target_limit(base::GetFieldTrialParamByFeatureAsInt(
        app_list_features::kEnableAdaptiveResultRanker, "fallback_target_limit",
        200));
    fallback->set_decay_coeff(base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableAdaptiveResultRanker, "fallback_decay_coeff",
        0.8f));

    results_list_group_ranker_ = std::make_unique<RecurrenceRanker>(
        profile->GetPath().AppendASCII("adaptive_result_ranker.proto"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile));

    results_list_boost_coefficient_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableAdaptiveResultRanker, "boost_coefficient",
        0.1);
  }

  profile_ = profile;
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile_))
    notifier->AddObserver(this);
  /*file_tasks_observer_.Add(
      file_manager::file_tasks::FileTasksNotifierFactory::GetInstance()
          ->GetForProfile(profile));*/
}

SearchResultRanker::~SearchResultRanker() {
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile_))
    notifier->RemoveObserver(this);
}

void SearchResultRanker::FetchRankings(const base::string16& query) {
  // The search controller potentially calls SearchController::FetchResults
  // several times for each user's search, so we cache the results of querying
  // the models for a short time, to prevent uneccessary queries.
  const auto& now = Time::Now();
  if (now - time_of_last_fetch_ < kMinSecondsBetweenFetches)
    return;
  time_of_last_fetch_ = now;

  // TODO(931149): The passed |query| should be used to choose between ranking
  // results with using a zero-state or query-based model.

  if (results_list_group_ranker_) {
    group_ranks_.clear();
    group_ranks_ = results_list_group_ranker_->Rank();
  }
}

void SearchResultRanker::Rank(Mixer::SortedResults& results) {
  for (auto& result : results) {
    const RankingItemType& type =
        RankingItemTypeFromSearchResult(*result.result);
    const Model& model = ModelForType(type);

    if (model == Model::RESULTS_LIST_GROUP_RANKER &&
        results_list_group_ranker_) {
      const auto& rank_it =
          group_ranks_.find(base::NumberToString(static_cast<int>(type)));
      // The ranker only contains entries trained with types relating to files
      // or the omnibox. This means scores for apps, app shortcuts, and answer
      // cards will be unchanged.
      if (rank_it != group_ranks_.end()) {
        // Ranker scores are guaranteed to be in [0,1]. But, enforce that the
        // result of tweaking does not put the score above 3.0, as that may
        // interfere with apps or answer cards.
        result.score = std::min(
            result.score + rank_it->second * results_list_boost_coefficient_,
            3.0);
      }
    }
  }
}

void SearchResultRanker::Train(const std::string& id, RankingItemType type) {
  const Model& model = ModelForType(type);

  if (model == Model::RESULTS_LIST_GROUP_RANKER && results_list_group_ranker_) {
    results_list_group_ranker_->Record(
        base::NumberToString(static_cast<int>(type)));
  }
}

void SearchResultRanker::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  // TODO(959679): route file open events to a model as training signals.
}

}  // namespace app_list
