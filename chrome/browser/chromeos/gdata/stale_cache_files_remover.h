// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_STALE_CACHE_FILE_REMOVER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_STALE_CACHE_FILE_REMOVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_interface.h"

namespace gdata{

class GDataCache;
class GDataFileSystem;

// This class removes stale cache files, which are present locally, but no
// longer present on the server. This can happen if files are removed from the
// server from other devices, or from the web interface.
class StaleCacheFilesRemover : public GDataFileSystemInterface::Observer {
 public:
  StaleCacheFilesRemover(GDataFileSystemInterface* file_system,
                         GDataCache* cache);
  virtual ~StaleCacheFilesRemover();

 private:
  // Removes stale cache files.
  // Gets the list of all the resource id and calls OnGetResourceIdsOfAllFiles()
  // with the list.
  virtual void OnInitialLoadFinished() OVERRIDE;

  // Gets the cache entry of each resource id. And passes the cache entry to
  // GetEntryInfoAndRemoveCacheIfNecessary().
  void OnGetResourceIdsOfAllFiles(const std::vector<std::string>& resource_ids);

  // Gets the file entry and calls RemoveCacheIfNecessary() with the file entry.
  // This is called from StaleCacheFilesRemover::OnGetResourceIdsOfAllFiles().
  void GetEntryInfoAndRemoveCacheIfNecessary(
      const std::string& resource_id,
      bool success,
      const gdata::GDataCacheEntry& cache_entry);

  // Check the cache file and removes if it is unavailable or invalid (eg. md5
  // mismatch). This is called from GetCacheEntryAndRemoveCacheIfNecessary();
  void RemoveCacheIfNecessary(
      const std::string& resource_id,
      const std::string& cache_md5,
      GDataFileError error,
      const FilePath& gdata_file_path,
      scoped_ptr<gdata::GDataEntryProto> entry_proto);

  GDataFileSystemInterface* file_system_;  // Not owned.
  GDataCache* cache_;  // Not owned.

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<StaleCacheFilesRemover> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(StaleCacheFilesRemover);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_STALE_CACHE_FILE_REMOVER_H_
