// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_area.h"

#include "content/common/storage_partition_service.mojom.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"

namespace content {

LocalStorageCachedArea::LocalStorageCachedArea(
    const url::Origin& origin,
    StoragePartitionService* storage_partition_service,
    LocalStorageCachedAreas* cached_areas)
    : loaded_(false), origin_(origin), binding_(this),
      cached_areas_(cached_areas) {
  storage_partition_service->OpenLocalStorage(
      origin_, mojo::GetProxy(&leveldb_));
}

LocalStorageCachedArea::~LocalStorageCachedArea() {
  cached_areas_->LocalStorageCacheAreaClosed(this);
}

unsigned LocalStorageCachedArea::GetLength() {
  EnsureLoaded();
  return 0u;
}

base::NullableString16 LocalStorageCachedArea::GetKey(unsigned index) {
  EnsureLoaded();
  return base::NullableString16();
}

base::NullableString16 LocalStorageCachedArea::GetItem(
    const base::string16& key) {
  EnsureLoaded();
  return base::NullableString16();
}

bool LocalStorageCachedArea::SetItem(const base::string16& key,
                                     const base::string16& value,
                                     const GURL& page_url) {
  EnsureLoaded();
  return false;
}

void LocalStorageCachedArea::RemoveItem(const base::string16& key,
                                        const GURL& page_url) {
  EnsureLoaded();
}

void LocalStorageCachedArea::Clear(const GURL& page_url) {
  // No need to prime the cache in this case.

  binding_.Close();
  // TODO:
  // binding_.CreateInterfacePtrAndBind()
}

void LocalStorageCachedArea::KeyChanged(mojo::Array<uint8_t> key,
                                        mojo::Array<uint8_t> new_value,
                                        mojo::Array<uint8_t> old_value,
                                        const mojo::String& source) {
}

void LocalStorageCachedArea::KeyDeleted(mojo::Array<uint8_t> key,
                                        const mojo::String& source) {
}

void LocalStorageCachedArea::AllDeleted(const mojo::String& source) {
}

void LocalStorageCachedArea::EnsureLoaded() {
  if (loaded_)
    return;

  loaded_ = true;
  leveldb::DatabaseError status = leveldb::DatabaseError::OK;
  mojo::Array<content::KeyValuePtr> data;
  leveldb_->GetAll(binding_.CreateInterfacePtrAndBind(), &status, &data);
}

}  // namespace content
