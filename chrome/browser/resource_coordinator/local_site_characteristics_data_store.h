// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_store.h"
#include "components/history/core/browser/history_service_observer.h"

class Profile;

namespace resource_coordinator {

// Implementation of a SiteCharacteristicsDataStore that use the local site
// characteristics database as a backend.
class LocalSiteCharacteristicsDataStore
    : public SiteCharacteristicsDataStore,
      public internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate,
      public history::HistoryServiceObserver {
 public:
  using LocalSiteCharacteristicsMap =
      base::flat_map<std::string, internal::LocalSiteCharacteristicsDataImpl*>;

  explicit LocalSiteCharacteristicsDataStore(Profile* profile);
  ~LocalSiteCharacteristicsDataStore() override;

  // SiteCharacteristicDataStore:
  std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      const std::string& origin_str) override;

  const LocalSiteCharacteristicsMap& origin_data_map_for_testing() const {
    return origin_data_map_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest, EndToEnd);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest,
                           HistoryServiceObserver);

  // Returns a pointer to the LocalSiteCharacteristicsDataImpl object
  // associated with |origin|, create one and add it to |origin_data_map_|
  // if it doesn't exist.
  internal::LocalSiteCharacteristicsDataImpl* GetOrCreateFeatureImpl(
      const std::string& origin_str);

  // internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate:
  void OnLocalSiteCharacteristicsDataImplDestroyed(
      internal::LocalSiteCharacteristicsDataImpl* impl) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionTimeRange& time_range,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Reset the observations about a given LocalSiteCharacteristicsMap entry,
  // removes this entry from the map if it's not loaded by any tab.
  // Returns an iterator to the next element in the map.
  LocalSiteCharacteristicsMap::iterator ResetLocalSiteCharacteristicsEntry(
      LocalSiteCharacteristicsMap::iterator entry);

  // Map a serialized origin to a LocalSiteCharacteristicDataInternal
  // pointer.
  LocalSiteCharacteristicsMap origin_data_map_;

  ScopedObserver<history::HistoryService, LocalSiteCharacteristicsDataStore>
      history_observer_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataStore);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_
