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

namespace identity {
class IdentityManager;
}  // namespace identity

// A service that downloads, caches, and hands out SearchSuggestData. It never
// initiates a download automatically, only when Refresh is called. When the
// user signs in or out, the cached value is cleared.
class SearchSuggestService : public KeyedService {
 public:
  SearchSuggestService(identity::IdentityManager* identity_manager,
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

  SearchSuggestLoader* loader_for_testing() { return loader_.get(); }

 private:
  class SigninObserver;

  void SigninStatusChanged();

  void SearchSuggestDataLoaded(SearchSuggestLoader::Status status,
                               const base::Optional<SearchSuggestData>& data);

  void NotifyObservers();

  std::unique_ptr<SearchSuggestLoader> loader_;

  std::unique_ptr<SigninObserver> signin_observer_;

  base::ObserverList<SearchSuggestServiceObserver, true>::Unchecked observers_;

  base::Optional<SearchSuggestData> search_suggest_data_;
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_H_
