// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;
using api::experimental_system_info_storage::StorageUnitInfo;

namespace systeminfo {

const char kStorageTypeUnknown[] = "unknown";
const char kStorageTypeHardDisk[] = "harddisk";
const char kStorageTypeRemovable[] = "removable";

}  // namespace systeminfo

const int kDefaultPollingIntervalMs = 1000;
const char kWatchingTokenName[] = "_storage_info_watching_token_";

StorageInfoProvider::StorageInfoProvider()
    : observers_(new ObserverListThreadSafe<StorageInfoObserver>()),
      watching_interval_(kDefaultPollingIntervalMs) {
  DCHECK(chrome::StorageMonitor::GetInstance());
  chrome::StorageMonitor::GetInstance()->AddObserver(this);
}

StorageInfoProvider::~StorageInfoProvider() {
  // Note that StorageInfoProvider is defined as a LazyInstance which would be
  // destroyed at process exiting, so its lifetime should be longer than the
  // StorageMonitor instance that would be destroyed before
  // StorageInfoProvider.
  if (chrome::StorageMonitor::GetInstance())
    chrome::StorageMonitor::GetInstance()->RemoveObserver(this);
}

void StorageInfoProvider::AddObserver(StorageInfoObserver* obs) {
  observers_->AddObserver(obs);
}

void StorageInfoProvider::RemoveObserver(StorageInfoObserver* obs) {
  observers_->RemoveObserver(obs);
}

void StorageInfoProvider::OnRemovableStorageAttached(
    const chrome::StorageInfo& info) {
  // Since the storage API uses the location as identifier, e.g.
  // the drive letter on Windows while mount point on Posix. Here we has to
  // convert the |info.location| to UTF-8 encoding.
  //
  // TODO(hongbo): use |info.device_id| instead of using |info.location|
  // to keep the id persisting between device attachments, like
  // StorageMonitor does.
#if defined(OS_POSIX)
  std::string id = info.location;
#elif defined(OS_WIN)
  std::string id = UTF16ToUTF8(info.location);
#endif
  // Post a task to blocking pool for querying the information.
  BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&StorageInfoProvider::QueryAttachedStorageInfoOnBlockingPool,
                 this, id));
}

void StorageInfoProvider::QueryAttachedStorageInfoOnBlockingPool(
    const std::string& id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  StorageUnitInfo info;
  if (!QueryUnitInfo(id, &info))
    return;
  observers_->Notify(&StorageInfoObserver::OnStorageAttached,
                     info.id,
                     info.type,
                     info.capacity,
                     info.available_capacity);
}

void StorageInfoProvider::OnRemovableStorageDetached(
    const chrome::StorageInfo& info) {
  // TODO(hongbo): Use |info.device_id| instead. Same as the above.
#if defined(OS_POSIX)
  std::string id = info.location;
#elif defined(OS_WIN)
  std::string id = UTF16ToUTF8(info.location);
#endif
  observers_->Notify(&StorageInfoObserver::OnStorageDetached, id);
}

void StorageInfoProvider::StartWatching(const std::string& id) {
  BrowserThread::PostBlockingPoolSequencedTask(
      kWatchingTokenName,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::AddWatchedStorageOnBlockingPool,
                 this, id));
}

void StorageInfoProvider::StopWatching(const std::string& id) {
  BrowserThread::PostBlockingPoolSequencedTask(
      kWatchingTokenName,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::RemoveWatchedStorageOnBlockingPool,
                 this, id));
}

void StorageInfoProvider::AddWatchedStorageOnBlockingPool(
    const std::string& id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  // If the storage |id| is already being watched.
  if (ContainsKey(storage_id_to_size_map_, id))
    return;

  StorageUnitInfo info;
  if (!QueryUnitInfo(id, &info))
    return;

  storage_id_to_size_map_[id] = info.available_capacity;

  // If it is the first storage to be watched, we need to start the watching
  // timer.
  if (storage_id_to_size_map_.size() == 1) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&StorageInfoProvider::StartWatchingTimerOnUIThread, this));
  }
}

void StorageInfoProvider::RemoveWatchedStorageOnBlockingPool(
    const std::string& id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  if (!ContainsKey(storage_id_to_size_map_, id))
    return;

  storage_id_to_size_map_.erase(id);

  // Stop watching timer if there is no storage to be watched.
  if (storage_id_to_size_map_.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&StorageInfoProvider::StopWatchingTimerOnUIThread, this));
  }
}

void StorageInfoProvider::CheckWatchedStorages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostBlockingPoolSequencedTask(kWatchingTokenName, FROM_HERE,
      base::Bind(&StorageInfoProvider::CheckWatchedStoragesOnBlockingPool,
                 this));
}

void StorageInfoProvider::CheckWatchedStoragesOnBlockingPool() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  for (StorageIDToSizeMap::iterator it = storage_id_to_size_map_.begin();
       it != storage_id_to_size_map_.end(); ) {
    StorageUnitInfo info;
    if (!QueryUnitInfo(it->first, &info)) {
      storage_id_to_size_map_.erase(it++);
      continue;
    }
    if (it->second != info.available_capacity) {
      observers_->Notify(&StorageInfoObserver::OnStorageFreeSpaceChanged,
                         it->first, /* storage id */
                         it->second, /* old free space value */
                         info.available_capacity /* new value */);
      it->second = info.available_capacity;
    }
    ++it;
  }

  if (storage_id_to_size_map_.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&StorageInfoProvider::StopWatchingTimerOnUIThread,
                   this));
    return;
  }
  OnCheckWatchedStoragesFinishedForTesting();
}

void StorageInfoProvider::StartWatchingTimerOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Start the watching timer if it is not running.
  if (!watching_timer_.IsRunning()) {
    watching_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(watching_interval_),
        this, &StorageInfoProvider::CheckWatchedStorages);
  }
}

void StorageInfoProvider::StopWatchingTimerOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  watching_timer_.Stop();
}

}  // namespace extensions
