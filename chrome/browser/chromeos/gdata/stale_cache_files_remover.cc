// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/stale_cache_files_remover.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

// Emits the log when the remove failed.
void EmitErrorLog(DriveFileError error,
                  const std::string& resource_id,
                  const std::string& md5) {
  if (error != gdata::DRIVE_FILE_OK) {
    LOG(WARNING) << "Failed to remove a stale cache file. resource_id:"
                 << resource_id;
  }
}

}  // namespace

StaleCacheFilesRemover::StaleCacheFilesRemover(
    DriveFileSystemInterface* file_system,
    DriveCache* cache)
    : cache_(cache),
      file_system_(file_system),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  file_system_->AddObserver(this);
}

StaleCacheFilesRemover::~StaleCacheFilesRemover() {
  file_system_->RemoveObserver(this);
}

void StaleCacheFilesRemover::OnInitialLoadFinished() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FilePath root_path = FilePath(gdata::kDriveRootDirectory);
  cache_->GetResourceIdsOfAllFilesOnUIThread(
      base::Bind(&StaleCacheFilesRemover::OnGetResourceIdsOfAllFiles,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StaleCacheFilesRemover::OnGetResourceIdsOfAllFiles(
    const std::vector<std::string>& resource_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < resource_ids.size(); ++i) {
    const std::string& resource_id = resource_ids[i];
    cache_->GetCacheEntryOnUIThread(
        resource_id,
        "",  // Don't check MD5.
        base::Bind(
            &StaleCacheFilesRemover::GetEntryInfoAndRemoveCacheIfNecessary,
            weak_ptr_factory_.GetWeakPtr(),
            resource_id));
  }
}

void StaleCacheFilesRemover::GetEntryInfoAndRemoveCacheIfNecessary(
    const std::string& resource_id,
    bool success,
    const gdata::DriveCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Removes the cache if GetCacheEntryOnUIThread() failed.
  if (!success) {
    LOG(WARNING) << "GetCacheEntryOnUIThread() failed";
    return;
  }

  file_system_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&StaleCacheFilesRemover::RemoveCacheIfNecessary,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id,
                 cache_entry.md5()));
}

void StaleCacheFilesRemover::RemoveCacheIfNecessary(
    const std::string& resource_id,
    const std::string& cache_md5,
    DriveFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<gdata::DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The entry is not found in the file system.
  if (error != gdata::DRIVE_FILE_OK) {
    cache_->RemoveOnUIThread(resource_id, base::Bind(&EmitErrorLog));
    return;
  }

  // The entry is found but the MD5 does not match.
  DCHECK(entry_proto.get());
  if (!entry_proto->has_file_specific_info() ||
      cache_md5 != entry_proto->file_specific_info().file_md5()) {
    cache_->RemoveOnUIThread(resource_id, base::Bind(&EmitErrorLog));
    return;
  }
}

}  // namespace gdata
