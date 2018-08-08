// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_task.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace background_fetch {

DatabaseTaskHost::DatabaseTaskHost() : weak_factory_(this) {}

DatabaseTaskHost::~DatabaseTaskHost() = default;

base::WeakPtr<DatabaseTaskHost> DatabaseTaskHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

DatabaseTask::DatabaseTask(DatabaseTaskHost* host) : host_(host) {
  DCHECK(host_);
  // Hold a reference to the CacheStorageManager.
  cache_manager_ = data_manager()->cache_manager();
}

DatabaseTask::~DatabaseTask() = default;

void DatabaseTask::Finished() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Post the OnTaskFinished callback to the same thread, to allow the the
  // DatabaseTask to finish execution before deallocating it.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DatabaseTaskHost::OnTaskFinished,
                                host_->GetWeakPtr(), this));
}

void DatabaseTask::OnTaskFinished(DatabaseTask* finished_subtask) {
  size_t erased = active_subtasks_.erase(finished_subtask);
  DCHECK_EQ(erased, 1u);
}

void DatabaseTask::AddDatabaseTask(std::unique_ptr<DatabaseTask> task) {
  DCHECK_EQ(task->host_, data_manager());
  data_manager()->AddDatabaseTask(std::move(task));
}

void DatabaseTask::AddSubTask(std::unique_ptr<DatabaseTask> task) {
  DCHECK_EQ(task->host_, this);
  auto insert_result = active_subtasks_.emplace(task.get(), std::move(task));
  insert_result.first->second->Start();  // Start the subtask.
}

void DatabaseTask::AbandonFetches(int64_t service_worker_registration_id) {
  for (auto& observer : data_manager()->observers())
    observer.OnServiceWorkerDatabaseCorrupted(service_worker_registration_id);
}

void DatabaseTask::SetStorageError(BackgroundFetchStorageError error) {
  DCHECK_NE(BackgroundFetchStorageError::kNone, error);
  switch (storage_error_) {
    case BackgroundFetchStorageError::kNone:
      storage_error_ = error;
      break;
    case BackgroundFetchStorageError::kServiceWorkerStorageError:
    case BackgroundFetchStorageError::kCacheStorageError:
      DCHECK(error == BackgroundFetchStorageError::kServiceWorkerStorageError ||
             error == BackgroundFetchStorageError::kCacheStorageError);
      if (storage_error_ != error)
        storage_error_ = BackgroundFetchStorageError::kStorageError;
      break;
    case BackgroundFetchStorageError::kStorageError:
      break;
  }
}

void DatabaseTask::SetStorageErrorAndFinish(BackgroundFetchStorageError error) {
  SetStorageError(error);
  FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
}

void DatabaseTask::ReportStorageError() {
  if (host_ != data_manager())
    return;  // This is a SubTask.

  base::UmaHistogramEnumeration("BackgroundFetch.Storage." + HistogramName(),
                                storage_error_);
}

std::string DatabaseTask::HistogramName() const {
  NOTREACHED() << "HistogramName needs to be provided.";
  return "GeneralDatabaseTask";
}

ServiceWorkerContextWrapper* DatabaseTask::service_worker_context() {
  DCHECK(data_manager()->service_worker_context());
  return data_manager()->service_worker_context();
}

CacheStorageManager* DatabaseTask::cache_manager() {
  DCHECK(cache_manager_);
  return cache_manager_.get();
}

std::set<std::string>& DatabaseTask::ref_counted_unique_ids() {
  return data_manager()->ref_counted_unique_ids();
}

ChromeBlobStorageContext* DatabaseTask::blob_storage_context() {
  return data_manager()->blob_storage_context();
}

BackgroundFetchDataManager* DatabaseTask::data_manager() {
  return host_->data_manager();
}

}  // namespace background_fetch

}  // namespace content
