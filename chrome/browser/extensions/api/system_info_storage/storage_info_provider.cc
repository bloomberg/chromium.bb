// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;
using api::experimental_system_info_storage::StorageUnitInfo;

namespace systeminfo {

const char kStorageTypeUnknown[] = "unknown";
const char kStorageTypeHardDisk[] = "harddisk";
const char kStorageTypeRemovable[] = "removable";

}  // namespace systeminfo

// Default watching interval is 1000 ms.
const int kDefaultPollingIntervalMs = 1000;
const char kWatchingTokenName[] = "_storage_info_watching_token_";

StorageInfoProvider::StorageInfoProvider()
    : observers_(new ObserverListThreadSafe<Observer>()),
      watching_interval_(kDefaultPollingIntervalMs) {
}

StorageInfoProvider::~StorageInfoProvider() {
}

void StorageInfoProvider::AddObserver(Observer* obs) {
  observers_->AddObserver(obs);
}

void StorageInfoProvider::RemoveObserver(Observer* obs) {
  observers_->RemoveObserver(obs);
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
      observers_->Notify(&Observer::OnStorageFreeSpaceChanged,
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
