// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/remove_stale_cache_files.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"

namespace drive {
namespace internal {

void RemoveStaleCacheFiles(FileCache* cache,
                           ResourceMetadata* resource_metadata) {
  scoped_ptr<ResourceMetadata::Iterator> it = resource_metadata->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const ResourceEntry& entry = it->GetValue();
    const FileCacheEntry& cache_state =
        entry.file_specific_info().cache_state();
    // Stale = not dirty but the MD5 does not match.
    if (!cache_state.is_dirty() &&
        cache_state.md5() != entry.file_specific_info().md5()) {
      FileError error = cache->Remove(it->GetID());
      LOG_IF(WARNING, error != FILE_ERROR_OK)
          << "Failed to remove a stale cache file. resource_id: "
          << it->GetID();
    }
  }
}

}  // namespace internal
}  // namespace drive
