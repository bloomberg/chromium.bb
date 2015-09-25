// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_MIGRATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_MIGRATOR_H_

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

// This is used for migrating the ServiceWorkerDiskCache from the BlockFile
// backend to the Simple backend. The migrator iterates over resources in the
// src DiskCache and copies them into the dest DiskCache one by one. This does
// not delete the resources in the src DiskCache.
//
// TODO(nhiroki): Remove this migrator after several milestones pass
// (http://crbug.com/487482)
class CONTENT_EXPORT ServiceWorkerDiskCacheMigrator {
 public:
  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode)>;

  ServiceWorkerDiskCacheMigrator(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      int max_disk_cache_size,
      const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread);
  ~ServiceWorkerDiskCacheMigrator();

  // Returns SERVICE_WORKER_OK if all resources are successfully migrated. The
  // caller should delete the src DiskCache after migration.
  void Start(const StatusCallback& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDiskCacheMigratorTest,
                           ThrottleInflightTasks);

  class Task;
  class WrappedEntry;

  using InflightTaskMap = IDMap<Task, IDMapOwnPointer>;

  // Used for UMA. Append only.
  enum class MigrationStatus {
    OK = 0,
    ERROR_FAILED = 1,  // unused, for backwards compatibility.
    ERROR_MOVE_DISKCACHE = 2,
    ERROR_INIT_SRC_DISKCACHE = 3,
    ERROR_INIT_DEST_DISKCACHE = 4,
    ERROR_OPEN_NEXT_ENTRY = 5,
    ERROR_READ_ENTRY_KEY = 6,
    ERROR_READ_RESPONSE_INFO = 7,
    ERROR_WRITE_RESPONSE_INFO = 8,
    ERROR_WRITE_RESPONSE_METADATA = 9,
    ERROR_READ_RESPONSE_DATA = 10,
    ERROR_WRITE_RESPONSE_DATA = 11,
    NUM_TYPES,
  };

#if defined(OS_ANDROID)
  static MigrationStatus MigrateForAndroid(const base::FilePath& src_path,
                                           const base::FilePath& dest_path);
#endif  // defined(OS_ANDROID)

  void DidDeleteDestDirectory(bool deleted);
  void DidInitializeSrcDiskCache(const base::Closure& barrier_closure,
                                 MigrationStatus* status_ptr,
                                 int result);
  void DidInitializeDestDiskCache(const base::Closure& barrier_closure,
                                  MigrationStatus* status_ptr,
                                  int result);
  void DidInitializeAllDiskCaches(scoped_ptr<MigrationStatus> status);

  void OpenNextEntry();
  void OnNextEntryOpened(scoped_ptr<WrappedEntry> entry, int result);
  void RunPendingTask();
  void OnEntryMigrated(InflightTaskMap::KeyType task_id,
                       MigrationStatus status);
  void Complete(MigrationStatus status);
  void RunUserCallback(ServiceWorkerStatusCode status);

  void RecordMigrationResult(MigrationStatus status);
  void RecordNumberOfMigratedResources(size_t migrated_resources);
  void RecordMigrationTime(const base::TimeDelta& time);

  void set_max_number_of_inflight_tasks(size_t max_number) {
    max_number_of_inflight_tasks_ = max_number;
  }

  scoped_ptr<disk_cache::Backend::Iterator> iterator_;
  bool is_iterating_ = false;

  base::FilePath src_path_;
  base::FilePath dest_path_;
  scoped_ptr<ServiceWorkerDiskCache> src_;
  scoped_ptr<ServiceWorkerDiskCache> dest_;
  const int max_disk_cache_size_;
  scoped_refptr<base::SingleThreadTaskRunner> disk_cache_thread_;

  InflightTaskMap::KeyType next_task_id_ = 0;
  InflightTaskMap inflight_tasks_;
  scoped_ptr<Task> pending_task_;
  size_t max_number_of_inflight_tasks_ = 10;

  base::TimeTicks start_time_;
  size_t number_of_migrated_resources_ = 0;

  StatusCallback callback_;

  base::WeakPtrFactory<ServiceWorkerDiskCacheMigrator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDiskCacheMigrator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_MIGRATOR_H_
