// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_task.h"

#include <utility>

#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace background_fetch {

DatabaseTask::DatabaseTask(BackgroundFetchDataManager* data_manager)
    : data_manager_(data_manager) {
  DCHECK(data_manager_);
  cache_manager_ = data_manager_->cache_manager();
}

DatabaseTask::DatabaseTask(BackgroundFetchDataManager* data_manager,
                           scoped_refptr<CacheStorageManager> cache_manager)
    : data_manager_(data_manager), cache_manager_(cache_manager) {
  DCHECK(data_manager_);
  DCHECK(cache_manager_);
}

DatabaseTask::~DatabaseTask() = default;

void DatabaseTask::Finished() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->OnDatabaseTaskFinished(this);
}

void DatabaseTask::AddDatabaseTask(std::unique_ptr<DatabaseTask> task) {
  data_manager_->AddDatabaseTask(std::move(task), true /* internal */);
}

ServiceWorkerContextWrapper* DatabaseTask::service_worker_context() {
  DCHECK(data_manager_->service_worker_context());
  return data_manager_->service_worker_context().get();
}

CacheStorageManager* DatabaseTask::cache_manager() {
  DCHECK(cache_manager_);
  return cache_manager_.get();
}

std::set<std::string>& DatabaseTask::ref_counted_unique_ids() {
  return data_manager_->ref_counted_unique_ids();
}

}  // namespace background_fetch

}  // namespace content
