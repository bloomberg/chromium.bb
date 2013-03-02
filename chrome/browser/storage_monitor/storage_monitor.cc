// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_monitor.h"

#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "chrome/browser/storage_monitor/transient_device_ids.h"

namespace chrome {

static StorageMonitor* g_storage_monitor = NULL;

StorageMonitor::StorageInfo::StorageInfo()
    : total_size_in_bytes(0) {
}

StorageMonitor::StorageInfo::StorageInfo(
    const std::string& id,
    const string16& device_name,
    const base::FilePath::StringType& device_location)
    : device_id(id),
      name(device_name),
      location(device_location),
      total_size_in_bytes(0) {
}

StorageMonitor::StorageInfo::~StorageInfo() {
}

StorageMonitor::Receiver::~Receiver() {
}

class StorageMonitor::ReceiverImpl : public StorageMonitor::Receiver {
 public:
  explicit ReceiverImpl(StorageMonitor* notifications)
      : notifications_(notifications) {}

  virtual ~ReceiverImpl() {}

  virtual void ProcessAttach(
      const StorageMonitor::StorageInfo& info) OVERRIDE;

  virtual void ProcessDetach(const std::string& id) OVERRIDE;

 private:
  StorageMonitor* notifications_;
};

void StorageMonitor::ReceiverImpl::ProcessAttach(
    const StorageMonitor::StorageInfo& info) {
  notifications_->ProcessAttach(info);
}

void StorageMonitor::ReceiverImpl::ProcessDetach(
    const std::string& id) {
  notifications_->ProcessDetach(id);
}

StorageMonitor* StorageMonitor::GetInstance() {
  return g_storage_monitor;
}

std::vector<StorageMonitor::StorageInfo>
StorageMonitor::GetAttachedStorage() const {
  std::vector<StorageInfo> results;

  base::AutoLock lock(storage_lock_);
  for (RemovableStorageMap::const_iterator it = storage_map_.begin();
       it != storage_map_.end();
       ++it) {
    results.push_back(it->second);
  }
  return results;
}

void StorageMonitor::AddObserver(RemovableStorageObserver* obs) {
  observer_list_->AddObserver(obs);
}

void StorageMonitor::RemoveObserver(
    RemovableStorageObserver* obs) {
  observer_list_->RemoveObserver(obs);
}

std::string StorageMonitor::GetTransientIdForDeviceId(
    const std::string& device_id) {
  return transient_device_ids_->GetTransientIdForDeviceId(device_id);
}

std::string StorageMonitor::GetDeviceIdForTransientId(
    const std::string& transient_id) const {
  return transient_device_ids_->DeviceIdFromTransientId(transient_id);
}

void StorageMonitor::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  // Platform-specific implementations will override this method to
  // perform actual device ejection.
  callback.Run(EJECT_FAILURE);
}

StorageMonitor::StorageMonitor()
    : observer_list_(new ObserverListThreadSafe<RemovableStorageObserver>()),
      transient_device_ids_(new TransientDeviceIds) {
  receiver_.reset(new ReceiverImpl(this));

  DCHECK(!g_storage_monitor);
  g_storage_monitor = this;
}

StorageMonitor::~StorageMonitor() {
  g_storage_monitor = NULL;
}

// static
void StorageMonitor::RemoveSingletonForTesting() {
  g_storage_monitor = NULL;
}

StorageMonitor::Receiver* StorageMonitor::receiver() const {
  return receiver_.get();
}

void StorageMonitor::ProcessAttach(
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

void StorageMonitor::ProcessDetach(const std::string& id) {
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
