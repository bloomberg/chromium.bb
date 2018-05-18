// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/leveldb_site_characteristics_database.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_writer.h"
#include "components/history/core/browser/history_service.h"

namespace resource_coordinator {

namespace {
constexpr char kSiteCharacteristicsDirectoryName[] =
    "Site Characteristics Database";
}

LocalSiteCharacteristicsDataStore::LocalSiteCharacteristicsDataStore(
    Profile* profile)
    : history_observer_(this) {
  database_ = std::make_unique<LevelDBSiteCharacteristicsDatabase>(
      profile->GetPath().AppendASCII(kSiteCharacteristicsDirectoryName));

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  if (history)
    history_observer_.Add(history);
}

LocalSiteCharacteristicsDataStore::~LocalSiteCharacteristicsDataStore() =
    default;

std::unique_ptr<SiteCharacteristicsDataReader>
LocalSiteCharacteristicsDataStore::GetReaderForOrigin(
    const std::string& origin_str) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetOrCreateFeatureImpl(origin_str);
  DCHECK(impl);
  SiteCharacteristicsDataReader* data_reader =
      new LocalSiteCharacteristicsDataReader(impl);
  return base::WrapUnique(data_reader);
}

std::unique_ptr<SiteCharacteristicsDataWriter>
LocalSiteCharacteristicsDataStore::GetWriterForOrigin(
    const std::string& origin_str) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetOrCreateFeatureImpl(origin_str);
  DCHECK(impl);
  LocalSiteCharacteristicsDataWriter* data_writer =
      new LocalSiteCharacteristicsDataWriter(impl);
  return base::WrapUnique(data_writer);
}

internal::LocalSiteCharacteristicsDataImpl*
LocalSiteCharacteristicsDataStore::GetOrCreateFeatureImpl(
    const std::string& origin_str) {
  // Start by checking if there's already an entry for this origin.
  auto iter = origin_data_map_.find(origin_str);
  if (iter != origin_data_map_.end())
    return iter->second;

  // If not create a new one and add it to the map.
  internal::LocalSiteCharacteristicsDataImpl* site_characteristic_data =
      new internal::LocalSiteCharacteristicsDataImpl(origin_str, this,
                                                     database_.get());

  // internal::LocalSiteCharacteristicsDataImpl is a ref-counted object, it's
  // safe to store a raw pointer to it here as this class will get notified when
  // it's about to be destroyed and it'll be removed from the map.
  origin_data_map_.insert(std::make_pair(origin_str, site_characteristic_data));
  return site_characteristic_data;
}

void LocalSiteCharacteristicsDataStore::
    OnLocalSiteCharacteristicsDataImplDestroyed(
        internal::LocalSiteCharacteristicsDataImpl* impl) {
  DCHECK(impl);
  DCHECK(base::ContainsKey(origin_data_map_, impl->origin_str()));
  // Remove the entry for this origin as this is about to get destroyed.
  auto num_erased = origin_data_map_.erase(impl->origin_str());
  DCHECK_EQ(1U, num_erased);
}

LocalSiteCharacteristicsDataStore::LocalSiteCharacteristicsMap::iterator
LocalSiteCharacteristicsDataStore::ResetLocalSiteCharacteristicsEntry(
    LocalSiteCharacteristicsMap::iterator entry) {
  if (entry->second->IsLoaded()) {
    entry->second->ClearObservations();
    entry++;
  } else {
    entry = origin_data_map_.erase(entry);
  }
  return entry;
}

void LocalSiteCharacteristicsDataStore::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(sebmarchand): Invalidates all the pending read/write operations to the
  // database once it's asynchronous.
  if (deletion_info.IsAllHistory()) {
    for (auto iter = origin_data_map_.begin();
         iter != origin_data_map_.end();) {
      iter = ResetLocalSiteCharacteristicsEntry(iter);
    }
    database_->ClearDatabase();
  } else {
    std::vector<std::string> entries_to_remove;
    for (auto deleted_row : deletion_info.deleted_rows()) {
      auto map_iter =
          origin_data_map_.find(deleted_row.url().GetOrigin().GetContent());
      if (map_iter != origin_data_map_.end()) {
        ResetLocalSiteCharacteristicsEntry(map_iter);
        entries_to_remove.emplace_back(map_iter->first);
      }
    }
    database_->RemoveSiteCharacteristicsFromDB(entries_to_remove);
  }
}

}  // namespace resource_coordinator
