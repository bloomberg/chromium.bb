// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/history_data.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/ui/app_list/search/history_data_observer.h"
#include "chrome/browser/ui/app_list/search/history_data_store.h"

namespace app_list {

namespace {

// A struct used to sort query entries by time.
struct EntrySortData {
  EntrySortData() : query(NULL), update_time(NULL) {}
  EntrySortData(const std::string* query,
                const base::Time* update_time)
      : query(query),
        update_time(update_time) {
  }

  const std::string* query;
  const base::Time* update_time;
};

bool EntrySortByTimeAscending(const EntrySortData& entry1,
                              const EntrySortData& entry2) {
  return *entry1.update_time < *entry2.update_time;
}

}  // namespace

HistoryData::Data::Data() {}
HistoryData::Data::~Data() {}

HistoryData::HistoryData(HistoryDataStore* store,
                         size_t max_primary,
                         size_t max_secondary)
    : store_(store),
      max_primary_(max_primary),
      max_secondary_(max_secondary) {
  store_->Load(base::Bind(&HistoryData::OnStoreLoaded, AsWeakPtr()));
}

HistoryData::~HistoryData() {}

void HistoryData::Add(const std::string& query, const std::string& result_id) {
  Associations::iterator assoc_it = associations_.find(query);

  // Creates a primary association if query is seen for the first time.
  if (assoc_it == associations_.end()) {
    Data& data = associations_[query];
    data.primary = result_id;
    data.update_time = base::Time::Now();

    store_->SetPrimary(query, result_id);
    store_->SetUpdateTime(query, data.update_time);

    TrimEntries();
    return;
  }

  Data& data = assoc_it->second;
  data.update_time = base::Time::Now();
  store_->SetUpdateTime(query, data.update_time);

  SecondaryDeque& secondary = data.secondary;
  if (!secondary.empty() && secondary.back() == result_id) {
    // Nothing to do if the last secondary is the current primary.
    if (data.primary == result_id)
      return;

    // If |result_id| is the last (the most recent) secondary and it's not the
    // current primary, promote it and demote the primary.
    secondary.pop_back();
    secondary.push_back(data.primary);
    data.primary = result_id;

    store_->SetPrimary(query, result_id);
    store_->SetSecondary(query, secondary);
    return;
  }

  // Otherwise, append to secondary list.
  SecondaryDeque::iterator secondary_it =
      std::find(secondary.begin(), secondary.end(), result_id);
  if (secondary_it != secondary.end())
    secondary.erase(secondary_it);

  secondary.push_back(result_id);
  if (secondary.size() > max_secondary_)
    secondary.pop_front();
  store_->SetSecondary(query, secondary);
}

scoped_ptr<KnownResults> HistoryData::GetKnownResults(
    const std::string& query) const {
  scoped_ptr<KnownResults> results(new KnownResults);
  for (Associations::const_iterator assoc_it =
           associations_.lower_bound(query);
       assoc_it != associations_.end();
       ++assoc_it) {
    // Break out of the loop if |query| is no longer a prefix.
    if (assoc_it->first.size() < query.size() ||
        strncmp(assoc_it->first.c_str(),
                query.c_str(),
                query.length()) != 0) {
      break;
    }

    const bool perfect = assoc_it->first == query;
    // Primary match should always be returned.
    (*results)[assoc_it->second.primary] =
        perfect ? PERFECT_PRIMARY : PREFIX_PRIMARY;

    const KnownResultType secondary_type =
        perfect ? PERFECT_SECONDARY : PREFIX_SECONDARY;
    const HistoryData::SecondaryDeque& secondary = assoc_it->second.secondary;
    for (HistoryData::SecondaryDeque::const_iterator secondary_it =
             secondary.begin();
         secondary_it != secondary.end();
         ++secondary_it) {
      const std::string& secondary_result_id = (*secondary_it);

      // Secondary match only gets added if there there is no primary match.
      if (results->find(secondary_result_id) != results->end())
        continue;
      (*results)[secondary_result_id] = secondary_type;
    }
  }

  return results.Pass();
}

void HistoryData::AddObserver(HistoryDataObserver* observer) {
  observers_.AddObserver(observer);
}

void HistoryData::RemoveObserver(HistoryDataObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HistoryData::OnStoreLoaded(scoped_ptr<Associations> loaded_data) {
  if (loaded_data)
    loaded_data->swap(associations_);

  FOR_EACH_OBSERVER(HistoryDataObserver, observers_,
                    OnHistoryDataLoadedFromStore());
}

void HistoryData::TrimEntries() {
  if (associations_.size() <= max_primary_)
    return;

  std::vector<EntrySortData> entries;
  for (Associations::const_iterator it = associations_.begin();
       it != associations_.end(); ++it) {
    entries.push_back(EntrySortData(&it->first, &it->second.update_time));
  }

  const size_t entries_to_remove = associations_.size() - max_primary_;
  std::partial_sort(entries.begin(),
                    entries.begin() + entries_to_remove,
                    entries.end(),
                    &EntrySortByTimeAscending);

  for (size_t i = 0; i < entries_to_remove; ++i) {
    const std::string& query = *entries[i].query;
    store_->Delete(query);
    associations_.erase(query);
  }
}

}  // namespace app_list
