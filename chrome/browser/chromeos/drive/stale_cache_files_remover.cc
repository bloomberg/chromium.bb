// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/stale_cache_files_remover.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// Emits the log when the remove failed.
void EmitErrorLog(const std::string& resource_id,
                  FileError error) {
  if (error != FILE_ERROR_OK) {
    LOG(WARNING) << "Failed to remove a stale cache file. resource_id:"
                 << resource_id;
  }
}

}  // namespace

StaleCacheFilesRemover::StaleCacheFilesRemover(
    FileSystemInterface* file_system,
    FileCache* cache)
    : cache_(cache),
      file_system_(file_system),
      weak_ptr_factory_(this) {
  file_system_->AddObserver(this);
}

StaleCacheFilesRemover::~StaleCacheFilesRemover() {
  file_system_->RemoveObserver(this);
}

void StaleCacheFilesRemover::OnInitialLoadFinished() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cache_->Iterate(
      base::Bind(
          &StaleCacheFilesRemover::GetEntryInfoAndRemoveCacheIfNecessary,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&base::DoNothing));
}

void StaleCacheFilesRemover::GetEntryInfoAndRemoveCacheIfNecessary(
    const std::string& resource_id,
    const FileCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
    FileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The entry is not found in the file system.
  if (error != FILE_ERROR_OK) {
    cache_->Remove(resource_id, base::Bind(&EmitErrorLog, resource_id));
    return;
  }

  // The entry is found but the MD5 does not match.
  DCHECK(entry.get());
  if (!entry->has_file_specific_info() ||
      cache_md5 != entry->file_specific_info().file_md5()) {
    cache_->Remove(resource_id, base::Bind(&EmitErrorLog, resource_id));
    return;
  }
}

}  // namespace drive
