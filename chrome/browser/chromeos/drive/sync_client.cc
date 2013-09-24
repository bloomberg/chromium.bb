// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync_client.h"

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/update_operation.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {

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

// The delay constant is used to delay retrying a sync task on server errors.
const int kLongDelaySeconds = 600;

// Iterates cache entries and appends IDs to |to_fetch| if the file is pinned
// but not fetched (not present locally), or to |to_upload| if the file is dirty
// but not uploaded.
void CollectBacklog(FileCache* cache,
                    std::vector<std::string>* to_fetch,
                    std::vector<std::string>* to_upload) {
  DCHECK(to_fetch);
  DCHECK(to_upload);

  scoped_ptr<FileCache::Iterator> it = cache->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const FileCacheEntry& cache_entry = it->GetValue();
    const std::string& local_id = it->GetID();
    if (cache_entry.is_pinned() && !cache_entry.is_present())
      to_fetch->push_back(local_id);

    if (cache_entry.is_dirty())
      to_upload->push_back(local_id);
  }
  DCHECK(!it->HasError());
}

// Iterates cache entries and collects IDs of ones with obsolete cache files.
void CheckExistingPinnedFiles(ResourceMetadata* metadata,
                              FileCache* cache,
                              std::vector<std::string>* local_ids) {
  scoped_ptr<FileCache::Iterator> it = cache->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const FileCacheEntry& cache_entry = it->GetValue();
    const std::string& local_id = it->GetID();
    if (!cache_entry.is_pinned() || !cache_entry.is_present())
      continue;

    ResourceEntry entry;
    FileError error = metadata->GetResourceEntryById(local_id, &entry);
    if (error != FILE_ERROR_OK) {
      LOG(WARNING) << "Entry not found: " << local_id;
      continue;
    }

    // If MD5s don't match, it indicates the local cache file is stale, unless
    // the file is dirty (the MD5 is "local"). We should never re-fetch the
    // file when we have a locally modified version.
    if (entry.file_specific_info().md5() == cache_entry.md5() ||
        cache_entry.is_dirty())
      continue;

    error = cache->Remove(local_id);
    if (error != FILE_ERROR_OK) {
      LOG(WARNING) << "Failed to remove cache entry: " << local_id;
      continue;
    }

    error = cache->Pin(local_id);
    if (error != FILE_ERROR_OK) {
      LOG(WARNING) << "Failed to pin cache entry: " << local_id;
      continue;
    }

    local_ids->push_back(local_id);
  }
  DCHECK(!it->HasError());
}

}  // namespace

SyncClient::SyncClient(base::SequencedTaskRunner* blocking_task_runner,
                       file_system::OperationObserver* observer,
                       JobScheduler* scheduler,
                       ResourceMetadata* metadata,
                       FileCache* cache,
                       const base::FilePath& temporary_file_directory)
    : blocking_task_runner_(blocking_task_runner),
      metadata_(metadata),
      cache_(cache),
      download_operation_(new file_system::DownloadOperation(
          blocking_task_runner,
          observer,
          scheduler,
          metadata,
          cache,
          temporary_file_directory)),
      update_operation_(new file_system::UpdateOperation(blocking_task_runner,
                                                         observer,
                                                         scheduler,
                                                         metadata,
                                                         cache)),
      delay_(base::TimeDelta::FromSeconds(kDelaySeconds)),
      long_delay_(base::TimeDelta::FromSeconds(kLongDelaySeconds)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SyncClient::~SyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SyncClient::StartProcessingBacklog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string>* to_fetch = new std::vector<std::string>;
  std::vector<std::string>* to_upload = new std::vector<std::string>;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CollectBacklog, cache_, to_fetch, to_upload),
      base::Bind(&SyncClient::OnGetLocalIdsOfBacklog,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(to_fetch),
                 base::Owned(to_upload)));
}

void SyncClient::StartCheckingExistingPinnedFiles() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string>* local_ids = new std::vector<std::string>;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CheckExistingPinnedFiles,
                 metadata_,
                 cache_,
                 local_ids),
      base::Bind(&SyncClient::AddFetchTasks,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(local_ids)));
}

void SyncClient::AddFetchTask(const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(FETCH, local_id, delay_);
}

void SyncClient::RemoveFetchTask(const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): Cancel tasks in JobScheduler as well. crbug.com/248856
  pending_fetch_list_.erase(local_id);
}

void SyncClient::AddUploadTask(const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(UPLOAD, local_id, delay_);
}

void SyncClient::AddTaskToQueue(SyncType type,
                                const std::string& local_id,
                                const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the same task is already queued, ignore this task.
  switch (type) {
    case FETCH:
      if (fetch_list_.find(local_id) == fetch_list_.end()) {
        fetch_list_.insert(local_id);
        pending_fetch_list_.insert(local_id);
      } else {
        return;
      }
      break;
    case UPLOAD:
    case UPLOAD_NO_CONTENT_CHECK:
      if (upload_list_.find(local_id) == upload_list_.end()) {
        upload_list_.insert(local_id);
      } else {
        return;
      }
      break;
  }

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SyncClient::StartTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 type,
                 local_id),
      delay);
}

void SyncClient::StartTask(SyncType type, const std::string& local_id) {
  switch (type) {
    case FETCH:
      // Check if the resource has been removed from the start list.
      if (pending_fetch_list_.find(local_id) != pending_fetch_list_.end()) {
        DVLOG(1) << "Fetching " << local_id;
        pending_fetch_list_.erase(local_id);

        download_operation_->EnsureFileDownloadedByLocalId(
            local_id,
            ClientContext(BACKGROUND),
            GetFileContentInitializedCallback(),
            google_apis::GetContentCallback(),
            base::Bind(&SyncClient::OnFetchFileComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       local_id));
      } else {
        // Cancel the task.
        fetch_list_.erase(local_id);
      }
      break;
    case UPLOAD:
    case UPLOAD_NO_CONTENT_CHECK:
      DVLOG(1) << "Uploading " << local_id;
      update_operation_->UpdateFileByLocalId(
          local_id,
          ClientContext(BACKGROUND),
          type == UPLOAD ? file_system::UpdateOperation::RUN_CONTENT_CHECK
                         : file_system::UpdateOperation::NO_CONTENT_CHECK,
          base::Bind(&SyncClient::OnUploadFileComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     local_id));
      break;
  }
}

void SyncClient::OnGetLocalIdsOfBacklog(
    const std::vector<std::string>* to_fetch,
    const std::vector<std::string>* to_upload) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Give priority to upload tasks over fetch tasks, so that dirty files are
  // uploaded as soon as possible.
  for (size_t i = 0; i < to_upload->size(); ++i) {
    const std::string& local_id = (*to_upload)[i];
    DVLOG(1) << "Queuing to upload: " << local_id;
    AddTaskToQueue(UPLOAD_NO_CONTENT_CHECK, local_id, delay_);
  }

  for (size_t i = 0; i < to_fetch->size(); ++i) {
    const std::string& local_id = (*to_fetch)[i];
    DVLOG(1) << "Queuing to fetch: " << local_id;
    AddTaskToQueue(FETCH, local_id, delay_);
  }
}

void SyncClient::AddFetchTasks(const std::vector<std::string>* local_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < local_ids->size(); ++i)
    AddFetchTask((*local_ids)[i]);
}

void SyncClient::OnFetchFileComplete(const std::string& local_id,
                                     FileError error,
                                     const base::FilePath& local_path,
                                     scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  fetch_list_.erase(local_id);

  if (error == FILE_ERROR_OK) {
    DVLOG(1) << "Fetched " << local_id << ": " << local_path.value();
  } else {
    switch (error) {
      case FILE_ERROR_ABORT:
        // If user cancels download, unpin the file so that we do not sync the
        // file again.
        cache_->UnpinOnUIThread(local_id,
                                base::Bind(&util::EmptyFileOperationCallback));
        break;
      case FILE_ERROR_NO_CONNECTION:
        // Re-queue the task so that we'll retry once the connection is back.
        AddTaskToQueue(FETCH, local_id, delay_);
        break;
      case FILE_ERROR_SERVICE_UNAVAILABLE:
        // Re-queue the task so that we'll retry once the service is back.
        AddTaskToQueue(FETCH, local_id, long_delay_);
        break;
      default:
        LOG(WARNING) << "Failed to fetch " << local_id
                     << ": " << FileErrorToString(error);
    }
  }
}

void SyncClient::OnUploadFileComplete(const std::string& local_id,
                                      FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  upload_list_.erase(local_id);

  if (error == FILE_ERROR_OK) {
    DVLOG(1) << "Uploaded " << local_id;
  } else {
    switch (error) {
      case FILE_ERROR_NO_CONNECTION:
        // Re-queue the task so that we'll retry once the connection is back.
        AddTaskToQueue(UPLOAD_NO_CONTENT_CHECK, local_id, delay_);
        break;
      case FILE_ERROR_SERVICE_UNAVAILABLE:
        // Re-queue the task so that we'll retry once the service is back.
        AddTaskToQueue(UPLOAD_NO_CONTENT_CHECK, local_id, long_delay_);
        break;
      default:
        LOG(WARNING) << "Failed to upload " << local_id << ": "
                     << FileErrorToString(error);
    }
  }
}

}  // namespace internal
}  // namespace drive
