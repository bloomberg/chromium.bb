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
#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"

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

// Iterates entries and appends IDs to |to_fetch| if the file is pinned but not
// fetched (not present locally), to |to_upload| if the file is dirty but not
// uploaded, or to |to_remove| if the entry is in the trash.
void CollectBacklog(ResourceMetadata* metadata,
                    std::vector<std::string>* to_fetch,
                    std::vector<std::string>* to_upload,
                    std::vector<std::string>* to_update) {
  DCHECK(to_fetch);
  DCHECK(to_upload);
  DCHECK(to_update);

  scoped_ptr<ResourceMetadata::Iterator> it = metadata->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    const std::string& local_id = it->GetID();
    const ResourceEntry& entry = it->GetValue();
    if (entry.parent_local_id() == util::kDriveTrashDirLocalId) {
      to_update->push_back(local_id);
      continue;
    }

    switch (entry.metadata_edit_state()) {
      case ResourceEntry::CLEAN:
        break;
      case ResourceEntry::SYNCING:
      case ResourceEntry::DIRTY:
        to_update->push_back(local_id);
        break;
    }

    FileCacheEntry cache_entry;
    if (it->GetCacheEntry(&cache_entry)) {
      if (cache_entry.is_pinned() && !cache_entry.is_present())
        to_fetch->push_back(local_id);

      if (cache_entry.is_dirty())
        to_upload->push_back(local_id);
    }
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

SyncClient::SyncTask::SyncTask() : state(PENDING), should_run_again(false) {}
SyncClient::SyncTask::~SyncTask() {}

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
      entry_update_performer_(new EntryUpdatePerformer(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata)),
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
  std::vector<std::string>* to_remove = new std::vector<std::string>;
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CollectBacklog, metadata_, to_fetch, to_upload, to_remove),
      base::Bind(&SyncClient::OnGetLocalIdsOfBacklog,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(to_fetch),
                 base::Owned(to_upload),
                 base::Owned(to_remove)));
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
  AddFetchTaskInternal(local_id, delay_);
}

void SyncClient::RemoveFetchTask(const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SyncTasks::iterator it = tasks_.find(SyncTasks::key_type(FETCH, local_id));
  if (it == tasks_.end())
    return;

  SyncTask* task = &it->second;
  switch (task->state) {
    case PENDING:
      tasks_.erase(it);
      break;
    case RUNNING:
      // TODO(kinaba): Cancel tasks in JobScheduler as well. crbug.com/248856
      break;
  }
}

void SyncClient::AddUploadTask(const ClientContext& context,
                               const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddUploadTaskInternal(context, local_id,
                        file_system::UpdateOperation::RUN_CONTENT_CHECK,
                        delay_);
}

void SyncClient::AddUpdateTask(const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddUpdateTaskInternal(local_id, base::TimeDelta::FromSeconds(0));
}

void SyncClient::AddFetchTaskInternal(const std::string& local_id,
                                      const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SyncTask task;
  task.task = base::Bind(
      &file_system::DownloadOperation::EnsureFileDownloadedByLocalId,
      base::Unretained(download_operation_.get()),
      local_id,
      ClientContext(BACKGROUND),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(&SyncClient::OnFetchFileComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id));
  AddTask(SyncTasks::key_type(FETCH, local_id), task, delay);
}

void SyncClient::AddUploadTaskInternal(
    const ClientContext& context,
    const std::string& local_id,
    file_system::UpdateOperation::ContentCheckMode content_check_mode,
    const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SyncTask task;
  task.task = base::Bind(
      &file_system::UpdateOperation::UpdateFileByLocalId,
      base::Unretained(update_operation_.get()),
      local_id,
      context,
      content_check_mode,
      base::Bind(&SyncClient::OnUploadFileComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id));
  AddTask(SyncTasks::key_type(UPLOAD, local_id), task, delay);
}

void SyncClient::AddUpdateTaskInternal(const std::string& local_id,
                                       const base::TimeDelta& delay) {
  SyncTask task;
  task.task = base::Bind(
      &EntryUpdatePerformer::UpdateEntry,
      base::Unretained(entry_update_performer_.get()),
      local_id,
      base::Bind(&SyncClient::OnUpdateComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id));
  AddTask(SyncTasks::key_type(UPDATE, local_id), task, delay);
}

void SyncClient::AddTask(const SyncTasks::key_type& key,
                         const SyncTask& task,
                         const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SyncTasks::iterator it = tasks_.find(key);
  if (it != tasks_.end()) {
    switch (it->second.state) {
      case PENDING:
        // The same task will run, do nothing.
        break;
      case RUNNING:
        // Something has changed since the task started. Schedule rerun.
        it->second.should_run_again = true;
        break;
    }
    return;
  }

  DCHECK_EQ(PENDING, task.state);
  tasks_[key] = task;

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SyncClient::StartTask, weak_ptr_factory_.GetWeakPtr(), key),
      delay);
}

void SyncClient::StartTask(const SyncTasks::key_type& key) {
  SyncTasks::iterator it = tasks_.find(key);
  if (it == tasks_.end())
    return;

  SyncTask* task = &it->second;
  switch (task->state) {
    case PENDING:
      task->state = RUNNING;
      task->task.Run();
      break;
    case RUNNING:  // Do nothing.
      break;
  }
}

void SyncClient::OnGetLocalIdsOfBacklog(
    const std::vector<std::string>* to_fetch,
    const std::vector<std::string>* to_upload,
    const std::vector<std::string>* to_update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Give priority to upload tasks over fetch tasks, so that dirty files are
  // uploaded as soon as possible.
  for (size_t i = 0; i < to_upload->size(); ++i) {
    const std::string& local_id = (*to_upload)[i];
    DVLOG(1) << "Queuing to upload: " << local_id;
    AddUploadTaskInternal(ClientContext(BACKGROUND), local_id,
                          file_system::UpdateOperation::NO_CONTENT_CHECK,
                          delay_);
  }

  for (size_t i = 0; i < to_fetch->size(); ++i) {
    const std::string& local_id = (*to_fetch)[i];
    DVLOG(1) << "Queuing to fetch: " << local_id;
    AddFetchTaskInternal(local_id, delay_);
  }

  for (size_t i = 0; i < to_update->size(); ++i) {
    const std::string& local_id = (*to_update)[i];
    DVLOG(1) << "Queuing to update: " << local_id;
    AddUpdateTask(local_id);
  }
}

void SyncClient::AddFetchTasks(const std::vector<std::string>* local_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < local_ids->size(); ++i)
    AddFetchTask((*local_ids)[i]);
}

bool SyncClient::OnTaskComplete(SyncType type, const std::string& local_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const SyncTasks::key_type key(type, local_id);
  SyncTasks::iterator it = tasks_.find(key);
  DCHECK(it != tasks_.end());

  if (it->second.should_run_again) {
    DVLOG(1) << "Running again: type = " << type << ", id = " << local_id;
    it->second.should_run_again = false;
    it->second.task.Run();
    return false;
  }

  tasks_.erase(it);
  return true;
}

void SyncClient::OnFetchFileComplete(const std::string& local_id,
                                     FileError error,
                                     const base::FilePath& local_path,
                                     scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!OnTaskComplete(FETCH, local_id))
    return;

  if (error == FILE_ERROR_OK) {
    DVLOG(1) << "Fetched " << local_id << ": " << local_path.value();
  } else {
    switch (error) {
      case FILE_ERROR_ABORT:
        // If user cancels download, unpin the file so that we do not sync the
        // file again.
        base::PostTaskAndReplyWithResult(
            blocking_task_runner_,
            FROM_HERE,
            base::Bind(&FileCache::Unpin, base::Unretained(cache_), local_id),
            base::Bind(&util::EmptyFileOperationCallback));
        break;
      case FILE_ERROR_NO_CONNECTION:
        // Add the task again so that we'll retry once the connection is back.
        AddFetchTaskInternal(local_id, delay_);
        break;
      case FILE_ERROR_SERVICE_UNAVAILABLE:
        // Add the task again so that we'll retry once the service is back.
        AddFetchTaskInternal(local_id, long_delay_);
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

  if (!OnTaskComplete(UPLOAD, local_id))
    return;

  if (error == FILE_ERROR_OK) {
    DVLOG(1) << "Uploaded " << local_id;
  } else {
    switch (error) {
      case FILE_ERROR_NO_CONNECTION:
        // Add the task again so that we'll retry once the connection is back.
        AddUploadTaskInternal(ClientContext(BACKGROUND), local_id,
                              file_system::UpdateOperation::NO_CONTENT_CHECK,
                              delay_);
        break;
      case FILE_ERROR_SERVICE_UNAVAILABLE:
        // Add the task again so that we'll retry once the service is back.
        AddUploadTaskInternal(ClientContext(BACKGROUND), local_id,
                              file_system::UpdateOperation::NO_CONTENT_CHECK,
                              long_delay_);
        break;
      default:
        LOG(WARNING) << "Failed to upload " << local_id << ": "
                     << FileErrorToString(error);
    }
  }
}

void SyncClient::OnUpdateComplete(const std::string& local_id,
                                  FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!OnTaskComplete(UPDATE, local_id))
    return;

  if (error == FILE_ERROR_OK) {
    DVLOG(1) << "Updated " << local_id;
  } else {
    switch (error) {
      case FILE_ERROR_NO_CONNECTION:
        // Add the task again so that we'll retry once the connection is back.
        AddUpdateTaskInternal(local_id, base::TimeDelta::FromSeconds(0));
        break;
      case FILE_ERROR_SERVICE_UNAVAILABLE:
        // Add the task again so that we'll retry once the service is back.
        AddUpdateTaskInternal(local_id, long_delay_);
        break;
      default:
        LOG(WARNING) << "Failed to update " << local_id << ": "
                     << FileErrorToString(error);
    }
  }
}

}  // namespace internal
}  // namespace drive
