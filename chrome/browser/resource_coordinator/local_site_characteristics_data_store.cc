// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/leveldb_site_characteristics_database.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_writer.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "components/history/core/browser/url_row.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

constexpr char kSiteCharacteristicsDirectoryName[] =
    "Site Characteristics Database";

size_t CountOriginsInURLRows(const history::URLRows& rows) {
  std::set<GURL> origins;
  for (auto& row : rows)
    origins.insert(row.url().GetOrigin());
  return origins.size();
}

}  // namespace

LocalSiteCharacteristicsDataStore::LocalSiteCharacteristicsDataStore(
    Profile* profile)
    : profile_(profile) {
  database_ = std::make_unique<LevelDBSiteCharacteristicsDatabase>(
      profile->GetPath().AppendASCII(kSiteCharacteristicsDirectoryName));

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  if (history)
    history_observer_.Add(history);

  // Register the debug interface against the profile.
  LocalSiteCharacteristicsDataStoreInspector::SetForProfile(this, profile);
}

LocalSiteCharacteristicsDataStore::~LocalSiteCharacteristicsDataStore() {
  LocalSiteCharacteristicsDataStoreInspector::SetForProfile(nullptr, profile_);
}

std::unique_ptr<SiteCharacteristicsDataReader>
LocalSiteCharacteristicsDataStore::GetReaderForOrigin(
    const url::Origin& origin) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetOrCreateFeatureImpl(origin);
  DCHECK(impl);
  SiteCharacteristicsDataReader* data_reader =
      new LocalSiteCharacteristicsDataReader(impl);
  return base::WrapUnique(data_reader);
}

std::unique_ptr<SiteCharacteristicsDataWriter>
LocalSiteCharacteristicsDataStore::GetWriterForOrigin(
    const url::Origin& origin,
    performance_manager::TabVisibility tab_visibility) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetOrCreateFeatureImpl(origin);
  DCHECK(impl);
  LocalSiteCharacteristicsDataWriter* data_writer =
      new LocalSiteCharacteristicsDataWriter(impl, tab_visibility);
  return base::WrapUnique(data_writer);
}

bool LocalSiteCharacteristicsDataStore::IsRecordingForTesting() {
  return true;
}

const char* LocalSiteCharacteristicsDataStore::GetDataStoreName() {
  return "LocalSiteCharacteristicsDataStore";
}

std::vector<url::Origin>
LocalSiteCharacteristicsDataStore::GetAllInMemoryOrigins() {
  std::vector<url::Origin> ret;

  ret.reserve(origin_data_map_.size());
  for (const auto& entry : origin_data_map_)
    ret.push_back(entry.first);

  return ret;
}

void LocalSiteCharacteristicsDataStore::GetDatabaseSize(
    DatabaseSizeCallback on_have_data) {
  database_->GetDatabaseSize(std::move(on_have_data));
}

bool LocalSiteCharacteristicsDataStore::GetDataForOrigin(
    const url::Origin& origin,
    bool* is_dirty,
    std::unique_ptr<SiteDataProto>* data) {
  DCHECK_NE(nullptr, data);
  const auto it = origin_data_map_.find(origin);
  if (it == origin_data_map_.end())
    return false;

  std::unique_ptr<SiteDataProto> ret = std::make_unique<SiteDataProto>();
  ret->CopyFrom(it->second->FlushStateToProto());
  *is_dirty = it->second->is_dirty();
  *data = std::move(ret);
  return true;
}

LocalSiteCharacteristicsDataStore*
LocalSiteCharacteristicsDataStore::GetDataStore() {
  return this;
}

internal::LocalSiteCharacteristicsDataImpl*
LocalSiteCharacteristicsDataStore::GetOrCreateFeatureImpl(
    const url::Origin& origin) {
  // Start by checking if there's already an entry for this origin.
  auto iter = origin_data_map_.find(origin);
  if (iter != origin_data_map_.end())
    return iter->second;

  // If not create a new one and add it to the map.
  internal::LocalSiteCharacteristicsDataImpl* site_characteristic_data =
      new internal::LocalSiteCharacteristicsDataImpl(origin, this,
                                                     database_.get());

  // internal::LocalSiteCharacteristicsDataImpl is a ref-counted object, it's
  // safe to store a raw pointer to it here as this class will get notified when
  // it's about to be destroyed and it'll be removed from the map.
  origin_data_map_.insert(std::make_pair(origin, site_characteristic_data));
  return site_characteristic_data;
}

void LocalSiteCharacteristicsDataStore::
    OnLocalSiteCharacteristicsDataImplDestroyed(
        internal::LocalSiteCharacteristicsDataImpl* impl) {
  DCHECK(impl);
  DCHECK(base::Contains(origin_data_map_, impl->origin()));
  // Remove the entry for this origin as this is about to get destroyed.
  auto num_erased = origin_data_map_.erase(impl->origin());
  DCHECK_EQ(1U, num_erased);
}

void LocalSiteCharacteristicsDataStore::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // It's not necessary to invalidate the pending DB write operations as they
  // run on a sequenced task and so it's guaranteed that the remove operations
  // posted here will run after any other pending operation.
  if (deletion_info.IsAllHistory()) {
    for (auto& data : origin_data_map_)
      data.second->ClearObservationsAndInvalidateReadOperation();
    database_->ClearDatabase();
  } else {
    std::vector<url::Origin> origins_to_remove;

    DCHECK_EQ(deletion_info.deleted_urls_origin_map().size(),
              CountOriginsInURLRows(deletion_info.deleted_rows()));
    for (const auto& it : deletion_info.deleted_urls_origin_map()) {
      const url::Origin origin = url::Origin::Create(it.first);
      const int remaining_visits_in_history = it.second.first;

      // If the origin no longer exists in history, clear the site
      // characteristics from memory and from the database.
      DCHECK_GE(remaining_visits_in_history, 0);
      if (remaining_visits_in_history == 0) {
        auto map_iter = origin_data_map_.find(origin);
        if (map_iter != origin_data_map_.end())
          map_iter->second->ClearObservationsAndInvalidateReadOperation();

        origins_to_remove.emplace_back(origin);
      }
    }

    if (!origins_to_remove.empty())
      database_->RemoveSiteCharacteristicsFromDB(origins_to_remove);
  }
}

void LocalSiteCharacteristicsDataStore::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observer_.Remove(history_service);
}

}  // namespace resource_coordinator
