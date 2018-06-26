// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_

#include <memory>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

class BackgroundFetchDataManager;
class CacheStorageManager;
class ChromeBlobStorageContext;
class ServiceWorkerContextWrapper;

// Note that this also handles non-error cases where the NONE is NONE.
using HandleBackgroundFetchErrorCallback =
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;

namespace background_fetch {

// A DatabaseTask is an asynchronous "transaction" that needs to read/write the
// Service Worker Database.
//
// Only one DatabaseTask can run at once per StoragePartition, and no other code
// reads/writes Background Fetch keys, so each task effectively has an exclusive
// lock, except that core Service Worker code may delete all keys for a
// ServiceWorkerRegistration or the entire database at any time.
class DatabaseTask {
 public:
  virtual ~DatabaseTask();

  virtual void Start() = 0;

 protected:
  explicit DatabaseTask(BackgroundFetchDataManager* data_manager);

  // Constructor for DatabaseTasks that are created by other DatabaseTasks.
  // They will need to provide their own copy of |cache_manager|, just in case
  // Shutdown was called and the CacheStorageManager reference in
  // |data_manager_| was invalidated.
  DatabaseTask(BackgroundFetchDataManager* data_manager,
               scoped_refptr<CacheStorageManager> cache_manager);

  // Each task MUST call this once finished, even if exceptions occur, to
  // release their lock and allow the next task to execute.
  void Finished();

  void AddDatabaseTask(std::unique_ptr<DatabaseTask> task);

  ServiceWorkerContextWrapper* service_worker_context();

  CacheStorageManager* cache_manager();

  std::set<std::string>& ref_counted_unique_ids();

  ChromeBlobStorageContext* blob_storage_context();

  BackgroundFetchDataManager* data_manager() { return data_manager_; }

 private:
  BackgroundFetchDataManager* data_manager_;  // Owns this.

  // Owns a reference to the CacheStorageManager in case Shutdown was
  // called and the DatabaseTask needs to finish.
  scoped_refptr<CacheStorageManager> cache_manager_;

  DISALLOW_COPY_AND_ASSIGN(DatabaseTask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_
