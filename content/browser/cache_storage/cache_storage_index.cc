// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_index.h"

#include <utility>

namespace content {

CacheStorageIndex::CacheStorageIndex()
    : doomed_cache_metadata_("", CacheStorage::kSizeUnknown) {
  ClearDoomedCache();
}

CacheStorageIndex::~CacheStorageIndex() = default;

CacheStorageIndex& CacheStorageIndex::operator=(CacheStorageIndex&& rhs) {
  DCHECK(!has_doomed_cache_);
  ordered_cache_metadata_ = std::move(rhs.ordered_cache_metadata_);
  cache_metadata_map_ = std::move(rhs.cache_metadata_map_);
  storage_size_ = rhs.storage_size_;
  rhs.storage_size_ = CacheStorage::kSizeUnknown;
  return *this;
}

void CacheStorageIndex::Insert(const CacheMetadata& cache_metadata) {
  DCHECK(!has_doomed_cache_);
  DCHECK(cache_metadata_map_.find(cache_metadata.name) ==
         cache_metadata_map_.end());
  ordered_cache_metadata_.push_back(cache_metadata);
  cache_metadata_map_[cache_metadata.name] = --ordered_cache_metadata_.end();
  storage_size_ = CacheStorage::kSizeUnknown;
}

void CacheStorageIndex::Delete(const std::string& cache_name) {
  DCHECK(!has_doomed_cache_);
  auto it = cache_metadata_map_.find(cache_name);
  DCHECK(it != cache_metadata_map_.end());
  ordered_cache_metadata_.erase(it->second);
  cache_metadata_map_.erase(it);
  storage_size_ = CacheStorage::kSizeUnknown;
}

bool CacheStorageIndex::SetCacheSize(const std::string& cache_name,
                                     int64_t size) {
  if (has_doomed_cache_)
    DCHECK_NE(cache_name, doomed_cache_metadata_.name);
  auto it = cache_metadata_map_.find(cache_name);
  DCHECK(it != cache_metadata_map_.end());
  if (it->second->size == size)
    return false;
  it->second->size = size;
  storage_size_ = CacheStorage::kSizeUnknown;
  return true;
}

int64_t CacheStorageIndex::GetCacheSize(const std::string& cache_name) const {
  const auto& it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end())
    return CacheStorage::kSizeUnknown;
  return it->second->size;
}

int64_t CacheStorageIndex::GetStorageSize() {
  if (storage_size_ == CacheStorage::kSizeUnknown)
    UpdateStorageSize();
  return storage_size_;
}

void CacheStorageIndex::UpdateStorageSize() {
  int64_t storage_size = 0;
  storage_size_ = CacheStorage::kSizeUnknown;
  for (const CacheMetadata& info : ordered_cache_metadata_) {
    if (info.size == CacheStorage::kSizeUnknown)
      return;
    storage_size += info.size;
  }
  storage_size_ = storage_size;
}

void CacheStorageIndex::DoomCache(const std::string& cache_name) {
  DCHECK(!has_doomed_cache_);
  auto map_it = cache_metadata_map_.find(cache_name);
  DCHECK(map_it != cache_metadata_map_.end());
  doomed_cache_metadata_ = std::move(*(map_it->second));
  after_doomed_cache_metadata_ = ordered_cache_metadata_.erase(map_it->second);
  cache_metadata_map_.erase(map_it);
  storage_size_ = CacheStorage::kSizeUnknown;
  has_doomed_cache_ = true;
}

void CacheStorageIndex::FinalizeDoomedCache() {
  DCHECK(has_doomed_cache_);
  ClearDoomedCache();
}

void CacheStorageIndex::RestoreDoomedCache() {
  DCHECK(has_doomed_cache_);
  const auto cache_name = doomed_cache_metadata_.name;
  cache_metadata_map_[cache_name] = ordered_cache_metadata_.insert(
      after_doomed_cache_metadata_, std::move(doomed_cache_metadata_));
  after_doomed_cache_metadata_ = ordered_cache_metadata_.end();
  storage_size_ = CacheStorage::kSizeUnknown;
  ClearDoomedCache();
}

void CacheStorageIndex::ClearDoomedCache() {
  doomed_cache_metadata_.name.clear();
  after_doomed_cache_metadata_ = ordered_cache_metadata_.end();
  has_doomed_cache_ = false;
}

}  // namespace content
