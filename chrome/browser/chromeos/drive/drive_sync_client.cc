// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_sync_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// The delay constant is used to delay processing a sync task. We should not
// process SyncTasks immediately for the following reasons:
//
// 1) For fetching, the user may accidentally click on "Make available
//    offline" checkbox on a file, and immediately cancel it in a second.
//    It's a waste to fetch the file in this case.
//
// 2) For uploading, file writing via HTML5 file system API is performed in
//    two steps: 1) truncate a file to 0 bytes, 2) write contents. We
//    shouldn't start uploading right after the step 1). Besides, the user
//    may edit the same file repeatedly in a short period of time.
//
// TODO(satorux): We should find a way to handle the upload case more nicely,
// and shorten the delay. crbug.com/134774
const int kDelaySeconds = 5;

// Appends |resource_id| to |to_fetch| if the file is pinned but not fetched
// (not present locally), or to |to_upload| if the file is dirty but not
// uploaded.
void CollectBacklog(std::vector<std::string>* to_fetch,
                    std::vector<std::string>* to_upload,
                    const std::string& resource_id,
                    const DriveCacheEntry& cache_entry) {
  DCHECK(to_fetch);
  DCHECK(to_upload);

  if (cache_entry.is_pinned() && !cache_entry.is_present())
    to_fetch->push_back(resource_id);

  if (cache_entry.is_dirty())
    to_upload->push_back(resource_id);
}

}  // namespace

DriveSyncClient::DriveSyncClient(Profile* profile,
                                 DriveFileSystemInterface* file_system,
                                 DriveCache* cache)
    : profile_(profile),
      file_system_(file_system),
      cache_(cache),
      delay_(base::TimeDelta::FromSeconds(kDelaySeconds)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveSyncClient::~DriveSyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (file_system_)
    file_system_->RemoveObserver(this);
  if (cache_)
    cache_->RemoveObserver(this);
}

void DriveSyncClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->AddObserver(this);
  cache_->AddObserver(this);
}

void DriveSyncClient::StartProcessingBacklog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string>* to_fetch = new std::vector<std::string>;
  std::vector<std::string>* to_upload = new std::vector<std::string>;
  cache_->Iterate(base::Bind(&CollectBacklog, to_fetch, to_upload),
                  base::Bind(&DriveSyncClient::OnGetResourceIdsOfBacklog,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Owned(to_fetch),
                             base::Owned(to_upload)));
}

void DriveSyncClient::StartCheckingExistingPinnedFiles() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cache_->Iterate(
      base::Bind(&DriveSyncClient::OnGetResourceIdOfExistingPinnedFile,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&base::DoNothing));
}

std::vector<std::string> DriveSyncClient::GetResourceIdsForTesting(
    SyncType sync_type) const {
  switch (sync_type) {
    case FETCH:
      return std::vector<std::string>(fetch_list_.begin(), fetch_list_.end());
    case UPLOAD:
      return std::vector<std::string>(upload_list_.begin(), upload_list_.end());
  }
  NOTREACHED();
  return std::vector<std::string>();
}

void DriveSyncClient::OnInitialLoadFinished(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == DRIVE_FILE_OK)
    StartProcessingBacklog();
}

void DriveSyncClient::OnFeedFromServerLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartCheckingExistingPinnedFiles();
}

void DriveSyncClient::OnCachePinned(const std::string& resource_id,
                                    const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(FETCH, resource_id);
}

void DriveSyncClient::OnCacheUnpinned(const std::string& resource_id,
                                      const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Remove the resource_id if it's in the queue. This can happen if the
  // user cancels pinning before the file is fetched.
  pending_fetch_list_.erase(resource_id);
}

void DriveSyncClient::OnCacheCommitted(const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(UPLOAD, resource_id);
}

void DriveSyncClient::AddTaskToQueue(SyncType type,
                                     const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the same task is already queued, ignore this task.
  switch (type) {
    case FETCH:
      if (fetch_list_.find(resource_id) == fetch_list_.end()) {
        fetch_list_.insert(resource_id);
        pending_fetch_list_.insert(resource_id);
      } else {
        return;
      }
      break;
    case UPLOAD:
      if (upload_list_.find(resource_id) == upload_list_.end()) {
        upload_list_.insert(resource_id);
      } else {
        return;
      }
      break;
  }

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DriveSyncClient::StartTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 type,
                 resource_id),
      delay_);
}

void DriveSyncClient::StartTask(SyncType type, const std::string& resource_id) {
  switch (type) {
    case FETCH:
      // Check if the resource has been removed from the start list.
      if (pending_fetch_list_.find(resource_id) != pending_fetch_list_.end()) {
        DVLOG(1) << "Fetching " << resource_id;
        pending_fetch_list_.erase(resource_id);

        file_system_->GetFileByResourceId(
            resource_id,
            DriveClientContext(BACKGROUND),
            base::Bind(&DriveSyncClient::OnFetchFileComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       resource_id),
            google_apis::GetContentCallback());
      } else {
        // Cancel the task.
        fetch_list_.erase(resource_id);
      }
      break;
    case UPLOAD:
      DVLOG(1) << "Uploading " << resource_id;
      file_system_->UpdateFileByResourceId(
          resource_id,
          DriveClientContext(BACKGROUND),
          base::Bind(&DriveSyncClient::OnUploadFileComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     resource_id));
      break;
  }
}

void DriveSyncClient::OnGetResourceIdsOfBacklog(
    const std::vector<std::string>* to_fetch,
    const std::vector<std::string>* to_upload) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Give priority to upload tasks over fetch tasks, so that dirty files are
  // uploaded as soon as possible.
  for (size_t i = 0; i < to_upload->size(); ++i) {
    const std::string& resource_id = (*to_upload)[i];
    DVLOG(1) << "Queuing to upload: " << resource_id;
    AddTaskToQueue(UPLOAD, resource_id);
  }

  for (size_t i = 0; i < to_fetch->size(); ++i) {
    const std::string& resource_id = (*to_fetch)[i];
    DVLOG(1) << "Queuing to fetch: " << resource_id;
    AddTaskToQueue(FETCH, resource_id);
  }
}

void DriveSyncClient::OnGetResourceIdOfExistingPinnedFile(
    const std::string& resource_id,
    const DriveCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (cache_entry.is_pinned() && cache_entry.is_present()) {
    file_system_->GetEntryInfoByResourceId(
        resource_id,
        base::Bind(&DriveSyncClient::OnGetEntryInfoByResourceId,
                   weak_ptr_factory_.GetWeakPtr(),
                   resource_id,
                   cache_entry));
  }
}

void DriveSyncClient::OnGetEntryInfoByResourceId(
    const std::string& resource_id,
    const DriveCacheEntry& cache_entry,
    DriveFileError error,
    const base::FilePath& /* drive_file_path */,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Entry not found: " << resource_id;
    return;
  }

  // If MD5s don't match, it indicates the local cache file is stale, unless
  // the file is dirty (the MD5 is "local"). We should never re-fetch the
  // file when we have a locally modified version.
  if (entry_proto->file_specific_info().file_md5() != cache_entry.md5() &&
      !cache_entry.is_dirty()) {
    cache_->Remove(resource_id,
                   base::Bind(&DriveSyncClient::OnRemove,
                              weak_ptr_factory_.GetWeakPtr(),
                              resource_id));
  }
}

void DriveSyncClient::OnRemove(const std::string& resource_id,
                               DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Failed to remove cache entry: " << resource_id;
    return;
  }

  // Before fetching, we should pin this file again, so that the fetched file
  // is downloaded properly to the persistent directory and marked pinned.
  cache_->Pin(resource_id,
              std::string(),
              base::Bind(&DriveSyncClient::OnPinned,
                         weak_ptr_factory_.GetWeakPtr(),
                         resource_id));
}

void DriveSyncClient::OnPinned(const std::string& resource_id,
                               DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Failed to pin cache entry: " << resource_id;
    return;
  }

  // Finally, adding to the queue.
  AddTaskToQueue(FETCH, resource_id);
}

void DriveSyncClient::OnFetchFileComplete(const std::string& resource_id,
                                          DriveFileError error,
                                          const base::FilePath& local_path,
                                          const std::string& ununsed_mime_type,
                                          DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  fetch_list_.erase(resource_id);

  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "Fetched " << resource_id << ": "
             << local_path.value();
  } else {
    switch (error) {
      case DRIVE_FILE_ERROR_NO_CONNECTION:
        // Re-queue the task so that we'll retry once the connection is back.
        AddTaskToQueue(FETCH, resource_id);
        break;
      default:
        LOG(WARNING) << "Failed to fetch " << resource_id
                     << ": " << error;
    }
  }
}

void DriveSyncClient::OnUploadFileComplete(const std::string& resource_id,
                                           DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  upload_list_.erase(resource_id);

  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "Uploaded " << resource_id;
  } else {
    // TODO(satorux): We should re-queue if the error is recoverable.
    LOG(WARNING) << "Failed to upload " << resource_id << ": " << error;
  }
}

}  // namespace drive
