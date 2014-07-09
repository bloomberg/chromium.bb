// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "ui/app_list/search_provider.h"

namespace app_list {

namespace {

// Maximum number of results to show.
const size_t kMaxResults = 6;
const size_t kMaxMainGroupResults = 4;
const size_t kMaxWebstoreResults = 2;
const size_t kMaxPeopleResults = 2;

// A value to indicate no max number of results limit.
const size_t kNoMaxResultsLimit = 0;

}  // namespace

Mixer::SortData::SortData() : result(NULL), score(0.0) {
}

Mixer::SortData::SortData(ChromeSearchResult* result, double score)
    : result(result), score(score) {
}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has higher score.
  return score > other.score;
}

// Used to group relevant providers together fox mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double boost)
      : max_results_(max_results),
        boost_(boost) {
  }
  ~Group() {}

  void AddProvider(SearchProvider* provider) {
    providers_.push_back(provider);
  }

  void FetchResults(const KnownResults& known_results) {
    results_.clear();

    for (Providers::const_iterator provider_it = providers_.begin();
         provider_it != providers_.end();
         ++provider_it) {
      for (SearchProvider::Results::const_iterator
               result_it = (*provider_it)->results().begin();
               result_it != (*provider_it)->results().end();
               ++result_it) {
        DCHECK_GE((*result_it)->relevance(), 0.0);
        DCHECK_LE((*result_it)->relevance(), 1.0);
        DCHECK(!(*result_it)->id().empty());

        double boost = boost_;
        KnownResults::const_iterator known_it =
            known_results.find((*result_it)->id());
        if (known_it != known_results.end()) {
          switch (known_it->second) {
            case PERFECT_PRIMARY:
              boost = 4.0;
              break;
            case PREFIX_PRIMARY:
              boost = 3.75;
              break;
            case PERFECT_SECONDARY:
              boost = 3.25;
              break;
            case PREFIX_SECONDARY:
              boost = 3.0;
              break;
            case UNKNOWN_RESULT:
              NOTREACHED() << "Unknown result in KnownResults?";
              break;
          }
        }

        results_.push_back(
            SortData(static_cast<ChromeSearchResult*>(*result_it),
                     (*result_it)->relevance() + boost));
      }
    }

    std::sort(results_.begin(), results_.end());
    if (max_results_ != kNoMaxResultsLimit && results_.size() > max_results_)
      results_.resize(max_results_);
  }

  const SortedResults& results() const { return results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;
  const double boost_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModel::SearchResults* ui_results)
    : ui_results_(ui_results) {}
Mixer::~Mixer() {}

void Mixer::Init() {
  groups_.push_back(new Group(kMaxMainGroupResults, 3.0));
  groups_.push_back(new Group(kNoMaxResultsLimit, 2.0));
  groups_.push_back(new Group(kMaxWebstoreResults, 1.0));
  groups_.push_back(new Group(kMaxPeopleResults, 0.0));
}

void Mixer::AddProviderToGroup(GroupId group, SearchProvider* provider) {
  size_t group_index = static_cast<size_t>(group);
  groups_[group_index]->AddProvider(provider);
}

void Mixer::MixAndPublish(const KnownResults& known_results) {
  FetchResults(known_results);

  SortedResults results;
  results.reserve(kMaxResults);

  // Adds main group and web store results first.
  results.insert(results.end(),
                 groups_[MAIN_GROUP]->results().begin(),
                 groups_[MAIN_GROUP]->results().end());
  results.insert(results.end(),
                 groups_[WEBSTORE_GROUP]->results().begin(),
                 groups_[WEBSTORE_GROUP]->results().end());
  results.insert(results.end(),
                 groups_[PEOPLE_GROUP]->results().begin(),
                 groups_[PEOPLE_GROUP]->results().end());

  // Collapse duplicate apps from local and web store.
  RemoveDuplicates(&results);

  DCHECK_GE(kMaxResults, results.size());
  size_t remaining_slots = kMaxResults - results.size();

  // Reserves at least one slot for the omnibox result. If there is no available
  // slot for omnibox results, removes the last one from web store.
  const size_t omnibox_results = groups_[OMNIBOX_GROUP]->results().size();
  if (!remaining_slots && omnibox_results)
    results.pop_back();

  remaining_slots = std::min(kMaxResults - results.size(), omnibox_results);
  results.insert(results.end(),
                 groups_[OMNIBOX_GROUP]->results().begin(),
                 groups_[OMNIBOX_GROUP]->results().begin() + remaining_slots);

  std::sort(results.begin(), results.end());
  RemoveDuplicates(&results);
  if (results.size() > kMaxResults)
    results.resize(kMaxResults);

  Publish(results, ui_results_);
}

void Mixer::Publish(const SortedResults& results,
                    AppListModel::SearchResults* ui_results) {
  for (size_t i = 0; i < results.size(); ++i) {
    ChromeSearchResult* result = results[i].result;

    ChromeSearchResult* ui_result =
        i < ui_results->item_count()
            ? static_cast<ChromeSearchResult*>(ui_results->GetItemAt(i))
            : NULL;
    if (ui_result && ui_result->id() == result->id()) {
      ui_result->set_title(result->title());
      ui_result->set_title_tags(result->title_tags());
      ui_result->set_details(result->details());
      ui_result->set_details_tags(result->details_tags());
      ui_results->NotifyItemsChanged(i, 1);
    } else {
      if (ui_result)
        ui_results->DeleteAt(i);
      ui_results->AddAt(i, result->Duplicate().release());
    }
  }

  while (ui_results->item_count() > results.size())
    ui_results->DeleteAt(ui_results->item_count() - 1);
}

void Mixer::RemoveDuplicates(SortedResults* results) {
  SortedResults final;
  final.reserve(results->size());

  std::set<std::string> id_set;
  for (SortedResults::iterator it = results->begin(); it != results->end();
       ++it) {
    const std::string& id = it->result->id();
    if (id_set.find(id) != id_set.end())
      continue;

    id_set.insert(id);
    final.push_back(*it);
  }

  results->swap(final);
}

void Mixer::FetchResults(const KnownResults& known_results) {
  for (Groups::iterator group_it = groups_.begin();
       group_it != groups_.end();
       ++group_it) {
    (*group_it)->FetchResults(known_results);
  }
}

}  // namespace app_list
