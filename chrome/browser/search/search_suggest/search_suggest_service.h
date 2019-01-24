// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_

#include <memory>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace identity {
class IdentityManager;
}  // namespace identity

// A service that downloads, caches, and hands out SearchSuggestData. It never
// initiates a download automatically, only when Refresh is called. When the
// user signs in or out, the cached value is cleared.
class SearchSuggestService : public KeyedService {
 public:
  SearchSuggestService(PrefService* pref_service,
                       identity::IdentityManager* identity_manager,
                       std::unique_ptr<SearchSuggestLoader> loader);
  ~SearchSuggestService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the currently cached SearchSuggestData, if any.
  const base::Optional<SearchSuggestData>& search_suggest_data() const {
    return search_suggest_data_;
  }

  // Clears any currently cached search suggest data.
  void ClearSearchSuggestData() { search_suggest_data_ = base::nullopt; }

  // Requests an asynchronous refresh from the network. After the update
  // completes, OnSearchSuggestDataUpdated will be called on the observers.
  void Refresh();

  // Add/remove observers. All observers must unregister themselves before the
  // SearchSuggestService is destroyed.
  void AddObserver(SearchSuggestServiceObserver* observer);
  void RemoveObserver(SearchSuggestServiceObserver* observer);

  // Register prefs associated with the NTP.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Add the task_id to the blacklist stored in user prefs. Overrides any
  // existing entry for the given task_id.
  //
  // A task_id represents a category of searches such as "Camping", a
  // task_version represents the selection criteria used to generate the
  // suggestion.
  void BlacklistSearchSuggestion(int task_version, long task_id);

  // Add the hash to the list of hashes for the task_id. Stored as a
  // dict of task_ids to lists of hashes in user prefs.
  //
  // A task_id represents a category of searches such as "Camping", a hash
  // is a specific search within the category such as "Camping equipment", and
  // a task_version represents the selection criteria used ti generate the
  // suggestion.
  void BlacklistSearchSuggestionWithHash(int task_version,
                                         long task_id,
                                         const std::vector<uint8_t>& hash);

  // Opt the current profile out of seeing search suggestions. Requests will
  // no longer be made.
  void OptOutOfSearchSuggestions();

  SearchSuggestLoader* loader_for_testing() { return loader_.get(); }

  // Returns the string representation of the suggestions blacklist in the form:
  // "task_id1:hash1,hash2,hash3;task_id2;task_id3:hash1,hash2".
  std::string GetBlacklistAsString();

 private:
  class SigninObserver;

  void SigninStatusChanged();

  void SearchSuggestDataLoaded(SearchSuggestLoader::Status status,
                               const base::Optional<SearchSuggestData>& data);

  void NotifyObservers();

  std::unique_ptr<SearchSuggestLoader> loader_;

  std::unique_ptr<SigninObserver> signin_observer_;

  PrefService* pref_service_;

  base::ObserverList<SearchSuggestServiceObserver, true>::Unchecked observers_;

  base::Optional<SearchSuggestData> search_suggest_data_;
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_
