// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

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

  ServiceWorkerDiskCacheMigrator(ServiceWorkerDiskCache* src,
                                 ServiceWorkerDiskCache* dest);
  ~ServiceWorkerDiskCacheMigrator();

  // Returns SERVICE_WORKER_OK if all resources are successfully migrated. The
  // caller should delete the src DiskCache after migration.
  void Start(const StatusCallback& callback);

 private:
  friend class ServiceWorkerDiskCacheMigratorTest;
  class Task;
  class WrappedEntry;

  using InflightTaskMap = IDMap<Task, IDMapOwnPointer>;

  void OpenNextEntry();
  void OnNextEntryOpened(scoped_ptr<WrappedEntry> entry, int result);
  void RunPendingTask();
  void OnEntryMigrated(InflightTaskMap::KeyType task_id,
                       ServiceWorkerStatusCode status);
  void Complete(ServiceWorkerStatusCode status);

  void set_max_number_of_inflight_tasks(size_t max_number) {
    max_number_of_inflight_tasks_ = max_number;
  }

  scoped_ptr<disk_cache::Backend::Iterator> iterator_;
  bool is_iterating_ = false;

  ServiceWorkerDiskCache* src_;
  ServiceWorkerDiskCache* dest_;

  InflightTaskMap::KeyType next_task_id_ = 0;
  InflightTaskMap inflight_tasks_;
  scoped_ptr<Task> pending_task_;
  size_t max_number_of_inflight_tasks_ = 10;

  StatusCallback callback_;

  base::WeakPtrFactory<ServiceWorkerDiskCacheMigrator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDiskCacheMigrator);
};

}  // namespace content
