// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"

namespace resource_coordinator {

LocalSiteCharacteristicsDataStore::LocalSiteCharacteristicsDataStore() =
    default;

LocalSiteCharacteristicsDataStore::~LocalSiteCharacteristicsDataStore() =
    default;

std::unique_ptr<SiteCharacteristicsDataReader>
LocalSiteCharacteristicsDataStore::GetReaderForOrigin(
    const std::string& origin_str) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetOrCreateFeatureImpl(origin_str);
  DCHECK_NE(nullptr, impl);
  SiteCharacteristicsDataReader* data_reader =
      new LocalSiteCharacteristicsDataReader(impl);
  return base::WrapUnique(data_reader);
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
      new internal::LocalSiteCharacteristicsDataImpl(origin_str, this);
  // internal::LocalSiteCharacteristicsDataImpl is a ref-counted object, it's
  // safe to store a raw pointer to it here as this class will get notified when
  // it's about to be destroyed and it'll be removed from the map.
  origin_data_map_.insert(std::make_pair(origin_str, site_characteristic_data));
  return site_characteristic_data;
}

void LocalSiteCharacteristicsDataStore::
    OnLocalSiteCharacteristicsDataImplDestroyed(
        internal::LocalSiteCharacteristicsDataImpl* impl) {
  DCHECK_NE(nullptr, impl);
  DCHECK(base::ContainsKey(origin_data_map_, impl->origin_str()));
  // Remove the entry for this origin as this is about to get destroyed.
  auto num_erased = origin_data_map_.erase(impl->origin_str());
  DCHECK_EQ(1U, num_erased);
}

}  // namespace resource_coordinator
