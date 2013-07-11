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

void RemoveStaleCacheFiles(FileCache* cache,
                           ResourceMetadata* resource_metadata) {
  std::vector<std::string> resource_ids_to_be_removed;

  scoped_ptr<FileCache::Iterator> it = cache->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    ResourceEntry entry;
    FileError error = resource_metadata->GetResourceEntryById(it->GetID(),
                                                              &entry);
    // Stale = the entry is not found, or not dirty but the MD5 does not match.
    if (error != FILE_ERROR_OK ||
        (!it->GetValue().is_dirty() &&
         it->GetValue().md5() != entry.file_specific_info().md5())) {
      FileError error = cache->Remove(it->GetID());
      LOG_IF(WARNING, error != FILE_ERROR_OK)
          << "Failed to remove a stale cache file. resource_id: "
          << it->GetID();
    }
  }
}

}  // namespace internal
}  // namespace drive
