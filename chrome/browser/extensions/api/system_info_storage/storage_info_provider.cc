// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include "base/stl_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace extensions {

using content::BrowserThread;
using api::experimental_system_info_storage::StorageUnitInfo;

namespace systeminfo {

const char kStorageTypeUnknown[] = "unknown";
const char kStorageTypeHardDisk[] = "harddisk";
const char kStorageTypeRemovable[] = "removable";

}  // namespace systeminfo

// Default watching interval is 1000 ms.
const unsigned int kDefaultPollingIntervalMs = 1000;

StorageInfoProvider::StorageInfoProvider()
    : watching_timer_(NULL),
      observers_(new ObserverListThreadSafe<Observer>()),
      watching_interval_(kDefaultPollingIntervalMs) {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

StorageInfoProvider::~StorageInfoProvider() {
  DCHECK(watching_timer_ == NULL);
  registrar_.RemoveAll();
}

void StorageInfoProvider::AddObserver(Observer* obs) {
  observers_->AddObserver(obs);
}

void StorageInfoProvider::RemoveObserver(Observer* obs) {
  observers_->RemoveObserver(obs);
}

void StorageInfoProvider::StartWatching(const std::string& id) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::StartWatchingOnFileThread,
                 base::Unretained(this), id));
}

void StorageInfoProvider::StopWatching(const std::string& id) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::StopWatchingOnFileThread,
                 base::Unretained(this), id));
}

void StorageInfoProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      BrowserThread::PostTask(BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&StorageInfoProvider::DestroyWatchingTimer,
                     base::Unretained(this)));
      break;
    default:
      NOTREACHED();
      break;
  }
}

void StorageInfoProvider::StartWatchingOnFileThread(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
    watching_timer_ = new base::RepeatingTimer<StorageInfoProvider>();
    watching_timer_->Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(watching_interval_),
        this, &StorageInfoProvider::CheckWatchedStorages);
  }
}

void StorageInfoProvider::StopWatchingOnFileThread(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!ContainsKey(storage_id_to_size_map_, id))
    return;

  storage_id_to_size_map_.erase(id);

  // If there is no storage to be watched, we need to destroy the watching
  // timer.
  if (storage_id_to_size_map_.size() == 0)
    DestroyWatchingTimer();
}

void StorageInfoProvider::CheckWatchedStorages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(storage_id_to_size_map_.size() > 0);

  StorageIDToSizeMap::iterator it = storage_id_to_size_map_.begin();
  for (; it != storage_id_to_size_map_.end(); ++it) {
    StorageUnitInfo info;
    if (!QueryUnitInfo(it->first, &info)) {
      storage_id_to_size_map_.erase(it);
      if (storage_id_to_size_map_.size() == 0) {
        DestroyWatchingTimer();
        return;
      }
    }
    if (it->second != info.available_capacity) {
      observers_->Notify(&Observer::OnStorageFreeSpaceChanged,
                         it->first, /* storage id */
                         it->second, /* old free space value */
                         info.available_capacity /* new value */);
      it->second = info.available_capacity;
    }
  }
}

void StorageInfoProvider::DestroyWatchingTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete watching_timer_;
  watching_timer_ = NULL;
}

}  // namespace extensions
