// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/remove_stale_cache_files.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"

namespace drive {
namespace internal {

namespace {

// Collects resource IDs of stale cache files.
void CollectStaleCacheFiles(
    ResourceMetadata* resource_metadata,
    std::vector<std::string>* out_resource_ids_to_be_removed,
    const std::string& resource_id,
    const FileCacheEntry& cache_entry) {
  ResourceEntry entry;
  FileError error = resource_metadata->GetResourceEntryById(
      resource_id, &entry);

  // The entry is not found or the MD5 does not match.
  if (error != FILE_ERROR_OK ||
      cache_entry.md5() != entry.file_specific_info().md5())
    out_resource_ids_to_be_removed->push_back(resource_id);
}

}  // namespace

void RemoveStaleCacheFiles(FileCache* cache,
                           ResourceMetadata* resource_metadata) {
  std::vector<std::string> resource_ids_to_be_removed;
  cache->Iterate(base::Bind(&CollectStaleCacheFiles,
                            resource_metadata,
                            &resource_ids_to_be_removed));

  for (size_t i = 0; i < resource_ids_to_be_removed.size(); ++i) {
    const std::string& resource_id = resource_ids_to_be_removed[i];
    FileError error = cache->Remove(resource_id);
    LOG_IF(WARNING, error != FILE_ERROR_OK)
        << "Failed to remove a stale cache file. resource_id: " << resource_id;
  }
}

}  // namespace internal
}  // namespace drive
