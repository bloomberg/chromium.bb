// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_STALE_CACHE_FILES_REMOVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_STALE_CACHE_FILES_REMOVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_observer.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace drive{

class DriveCache;
class DriveCacheEntry;
class DriveFileSystemInterface;

// This class removes stale cache files, which are present locally, but no
// longer present on the server. This can happen if files are removed from the
// server from other devices, or from the web interface.
class StaleCacheFilesRemover : public DriveFileSystemObserver {
 public:
  StaleCacheFilesRemover(DriveFileSystemInterface* file_system,
                         DriveCache* cache);
  virtual ~StaleCacheFilesRemover();

 private:
  // Removes stale cache files.
  // Gets the list of all the resource id and calls OnGetResourceIdsOfAllFiles()
  // with the list.
  virtual void OnInitialLoadFinished() OVERRIDE;

  // Gets the file entry and calls RemoveCacheIfNecessary() with the file entry.
  // This is called from StaleCacheFilesRemover::OnInitialLoadFinished.
  void GetEntryInfoAndRemoveCacheIfNecessary(
      const std::string& resource_id,
      const DriveCacheEntry& cache_entry);

  // Check the cache file and removes if it is unavailable or invalid (eg. md5
  // mismatch). This is called from GetCacheEntryAndRemoveCacheIfNecessary();
  void RemoveCacheIfNecessary(
      const std::string& resource_id,
      const std::string& cache_md5,
      DriveFileError error,
      const base::FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  DriveCache* cache_;  // Not owned.
  DriveFileSystemInterface* file_system_;  // Not owned.

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<StaleCacheFilesRemover> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(StaleCacheFilesRemover);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_STALE_CACHE_FILES_REMOVER_H_
