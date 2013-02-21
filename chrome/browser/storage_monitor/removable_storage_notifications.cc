// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/removable_storage_notifications.h"

#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "chrome/browser/storage_monitor/transient_device_ids.h"

namespace chrome {

static RemovableStorageNotifications*
    g_removable_storage_notifications = NULL;

RemovableStorageNotifications::StorageInfo::StorageInfo() {
}

RemovableStorageNotifications::StorageInfo::StorageInfo(
    const std::string& id,
    const string16& device_name,
    const base::FilePath::StringType& device_location)
    : device_id(id),
      name(device_name),
      location(device_location) {
}

RemovableStorageNotifications::Receiver::~Receiver() {
}

class RemovableStorageNotifications::ReceiverImpl
    : public RemovableStorageNotifications::Receiver {
 public:
  explicit ReceiverImpl(RemovableStorageNotifications* notifications)
      : notifications_(notifications) {}

  virtual ~ReceiverImpl() {}

  virtual void ProcessAttach(
      const std::string& id,
      const string16& name,
      const base::FilePath::StringType& location) OVERRIDE;

  virtual void ProcessDetach(const std::string& id) OVERRIDE;

 private:
  RemovableStorageNotifications* notifications_;
};

void RemovableStorageNotifications::ReceiverImpl::ProcessAttach(
    const std::string& id,
    const string16& name,
    const base::FilePath::StringType& location) {
  notifications_->ProcessAttach(
      RemovableStorageNotifications::StorageInfo(id, name, location));
}

void RemovableStorageNotifications::ReceiverImpl::ProcessDetach(
    const std::string& id) {
  notifications_->ProcessDetach(id);
}

RemovableStorageNotifications* RemovableStorageNotifications::GetInstance() {
  return g_removable_storage_notifications;
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

uint64 RemovableStorageNotifications::GetTransientIdForDeviceId(
    const std::string& device_id) {
  return transient_device_ids_->GetTransientIdForDeviceId(device_id);
}

std::string RemovableStorageNotifications::GetDeviceIdForTransientId(
    uint64 transient_id) const {
  return transient_device_ids_->DeviceIdFromTransientId(transient_id);
}

void RemovableStorageNotifications::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  // Platform-specific implementations will override this method to
  // perform actual device ejection.
  callback.Run(EJECT_FAILURE);
}

RemovableStorageNotifications::RemovableStorageNotifications()
    : observer_list_(new ObserverListThreadSafe<RemovableStorageObserver>()),
      transient_device_ids_(new TransientDeviceIds) {
  receiver_.reset(new ReceiverImpl(this));

  DCHECK(!g_removable_storage_notifications);
  g_removable_storage_notifications = this;
}

RemovableStorageNotifications::~RemovableStorageNotifications() {
  g_removable_storage_notifications = NULL;
}

// static
void RemovableStorageNotifications::RemoveSingletonForTesting() {
  g_removable_storage_notifications = NULL;
}

RemovableStorageNotifications::Receiver*
RemovableStorageNotifications::receiver() const {
  return receiver_.get();
}

void RemovableStorageNotifications::ProcessAttach(
    const StorageInfo& info) {
  {
    base::AutoLock lock(storage_lock_);
    if (ContainsKey(storage_map_, info.device_id)) {
      // This can happen if our unique id scheme fails. Ignore the incoming
      // non-unique attachment.
      return;
    }
    storage_map_.insert(std::make_pair(info.device_id, info));
  }

  DVLOG(1) << "RemovableStorageAttached with name " << UTF16ToUTF8(info.name)
           << " and id " << info.device_id;
  observer_list_->Notify(
      &RemovableStorageObserver::OnRemovableStorageAttached, info);
}

void RemovableStorageNotifications::ProcessDetach(const std::string& id) {
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

}  // namespace chrome
