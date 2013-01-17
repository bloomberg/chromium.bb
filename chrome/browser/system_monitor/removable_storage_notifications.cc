// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_storage_notifications.h"

#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/removable_storage_observer.h"

namespace chrome {

RemovableStorageNotifications::StorageInfo::StorageInfo() {
}

RemovableStorageNotifications::StorageInfo::StorageInfo(
    const std::string& id,
    const string16& device_name,
    const FilePath::StringType& device_location)
    : device_id(id),
      name(device_name),
      location(device_location) {
}

RemovableStorageNotifications::RemovableStorageNotifications()
    : observer_list_(
          new ObserverListThreadSafe<RemovableStorageObserver>()) {
}

RemovableStorageNotifications::~RemovableStorageNotifications() {
}

void RemovableStorageNotifications::ProcessAttach(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  StorageInfo info(id, name, location);
  {
    base::AutoLock lock(storage_lock_);
    if (ContainsKey(storage_map_, id)) {
      // This can happen if our unique id scheme fails. Ignore the incoming
      // non-unique attachment.
      return;
    }
    storage_map_.insert(std::make_pair(id, info));
  }

  DVLOG(1) << "RemovableStorageAttached with name " << UTF16ToUTF8(name)
           << " and id " << id;
  observer_list_->Notify(
      &RemovableStorageObserver::OnRemovableStorageAttached, info);
}

void RemovableStorageNotifications::ProcessDetach(
    const std::string& id) {
  StorageInfo info;
  {
    base::AutoLock lock(storage_lock_);
    RemovableStorageMap::iterator it = storage_map_.find(id);
    if (it == storage_map_.end())
      return;
    info = it->second;
    storage_map_.erase(it);
  }

  DVLOG(1) << "RemovableStorageDetached for id " << id;
  observer_list_->Notify(
      &RemovableStorageObserver::OnRemovableStorageDetached, info);
}

std::vector<RemovableStorageNotifications::StorageInfo>
RemovableStorageNotifications::GetAttachedStorage() const {
  std::vector<StorageInfo> results;

  base::AutoLock lock(storage_lock_);
  for (RemovableStorageMap::const_iterator it = storage_map_.begin();
       it != storage_map_.end();
       ++it) {
    results.push_back(it->second);
  }
  return results;
}

void RemovableStorageNotifications::AddObserver(RemovableStorageObserver* obs) {
  observer_list_->AddObserver(obs);
}

void RemovableStorageNotifications::RemoveObserver(
    RemovableStorageObserver* obs) {
  observer_list_->RemoveObserver(obs);
}

}  // namespace chrome
