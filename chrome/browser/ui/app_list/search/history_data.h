// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/history_types.h"

namespace app_list {

class HistoryDataObserver;
class HistoryDataStore;

// HistoryData stores the associations of the user typed queries and launched
// search result id. There are two types of association: primary and secondary.
// Primary is a 1-to-1 mapping between the query and result id. Secondary
// is a 1-to-many mapping and only kept the last 5 to limit the data size.
// If an association is added for the first time, it is added as a primary
// association. Further associations added to the same query are added as
// secondary. However, if a secondary association is added twice in a row, it
// is promoted to primary and the current primary mapping is demoted into
// secondary.
class HistoryData : public base::SupportsWeakPtr<HistoryData> {
 public:
  typedef std::deque<std::string> SecondaryDeque;

  // Defines data to be associated with a query.
  struct Data {
    Data();
    ~Data();

    // Primary result associated with the query.
    std::string primary;

    // Secondary results associated with the query from oldest to latest.
    SecondaryDeque secondary;

    // Last update time.
    base::Time update_time;
  };
  typedef std::map<std::string, Data> Associations;

  // Constructor of HistoryData. |store| is the storage to persist the data.
  // |max_primary| is the maximum number of the most recent primary associations
  // to keep. |max_secondary| is the maximum number of secondary associations to
  // keep.
  HistoryData(HistoryDataStore* store,
              size_t max_primary,
              size_t max_secondary);
  ~HistoryData();

  // Adds an association.
  void Add(const std::string& query, const std::string& result_id);

  // Gets all known search results that were launched using the given |query|
  // or the queries that |query| is a prefix of.
  scoped_ptr<KnownResults> GetKnownResults(const std::string& query) const;

  void AddObserver(HistoryDataObserver* observer);
  void RemoveObserver(HistoryDataObserver* observer);

  const Associations& associations() const { return associations_; }

 private:
  // Invoked from |store| with loaded data.
  void OnStoreLoaded(scoped_ptr<Associations> loaded_data);

  // Trims the data to keep the most recent |max_primary_| queries.
  void TrimEntries();

  HistoryDataStore* store_;  // Not owned.
  const size_t max_primary_;
  const size_t max_secondary_;
  ObserverList<HistoryDataObserver, true> observers_;

  Associations associations_;

  DISALLOW_COPY_AND_ASSIGN(HistoryData);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_H_
