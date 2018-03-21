// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

namespace app_list {

namespace {

const std::string& GetComparableId(const SearchResult& result) {
  return !result.comparable_id().empty() ? result.comparable_id() : result.id();
}

}  // namespace

Mixer::SortData::SortData() : result(nullptr), score(0.0) {}

Mixer::SortData::SortData(SearchResult* result, double score)
    : result(result), score(score) {}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has higher score.
  return score > other.score;
}

// Used to group relevant providers together for mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double multiplier, double boost)
      : max_results_(max_results), multiplier_(multiplier), boost_(boost) {}
  ~Group() {}

  void AddProvider(SearchProvider* provider) {
    providers_.emplace_back(provider);
  }

  void FetchResults(const KnownResults& known_results) {
    results_.clear();

    for (const SearchProvider* provider : providers_) {
      for (const auto& result : provider->results()) {
        DCHECK(!result->id().empty());

        // We cannot rely on providers to give relevance scores in the range
        // [0.0, 1.0]. Clamp to that range.
        const double relevance =
            std::min(std::max(result->relevance(), 0.0), 1.0);
        double boost = boost_;
        results_.emplace_back(result.get(), relevance * multiplier_ + boost);
      }
    }

    std::sort(results_.begin(), results_.end());
  }

  const SortedResults& results() const { return results_; }

  size_t max_results() const { return max_results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;
  const double multiplier_;
  const double boost_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModelUpdater* model_updater)
    : model_updater_(model_updater) {}

Mixer::~Mixer() = default;

size_t Mixer::AddGroup(size_t max_results, double multiplier, double boost) {
  groups_.push_back(std::make_unique<Group>(max_results, multiplier, boost));
  return groups_.size() - 1;
}

void Mixer::AddProviderToGroup(size_t group_id, SearchProvider* provider) {
  groups_[group_id]->AddProvider(provider);
}

void Mixer::MixAndPublish(const KnownResults& known_results,
                          size_t num_max_results) {
  FetchResults(known_results);

  SortedResults results;
  results.reserve(num_max_results);

  // Add results from each group. Limit to the maximum number of results in each
  // group.
  for (const auto& group : groups_) {
    const size_t num_results =
        std::min(group->results().size(), group->max_results());
    results.insert(results.end(), group->results().begin(),
                   group->results().begin() + num_results);
  }
  // Remove results with duplicate IDs before sorting. If two providers give a
  // result with the same ID, the result from the provider with the *lower group
  // number* will be kept (e.g., an app result takes priority over a web store
  // result with the same ID).
  RemoveDuplicates(&results);
  std::sort(results.begin(), results.end());

  const size_t original_size = results.size();
  if (original_size < num_max_results) {
    // We didn't get enough results. Insert all the results again, and this
    // time, do not limit the maximum number of results from each group. (This
    // will result in duplicates, which will be removed by RemoveDuplicates.)
    for (const auto& group : groups_) {
      results.insert(results.end(), group->results().begin(),
                     group->results().end());
    }
    RemoveDuplicates(&results);
    // Sort just the newly added results. This ensures that, for example, if
    // there are 6 Omnibox results (score = 0.8) and 1 People result (score =
    // 0.4) that the People result will be 5th, not 7th, because the Omnibox
    // group has a soft maximum of 4 results. (Otherwise, the People result
    // would not be seen at all once the result list is truncated.)
    std::sort(results.begin() + original_size, results.end());
  }

  std::vector<std::unique_ptr<app_list::SearchResult>> new_results;
  for (const SortData& sort_data : results) {
    std::unique_ptr<app_list::SearchResult> new_result =
        sort_data.result->Duplicate();
    new_result->set_relevance(sort_data.score);
    new_results.push_back(std::move(new_result));
  }
  model_updater_->PublishSearchResults(std::move(new_results));
}

void Mixer::RemoveDuplicates(SortedResults* results) {
  SortedResults final;
  final.reserve(results->size());

  std::set<std::string> id_set;
  for (const SortData& sort_data : *results) {
    if (!id_set.insert(GetComparableId(*sort_data.result)).second)
      continue;

    final.emplace_back(sort_data);
  }

  results->swap(final);
}

void Mixer::FetchResults(const KnownResults& known_results) {
  for (const auto& group : groups_)
    group->FetchResults(known_results);
}

}  // namespace app_list
