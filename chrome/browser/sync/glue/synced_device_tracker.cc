// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_device_tracker.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/util/time.h"

namespace browser_sync {

namespace {

// Return the DeviceInfo UNIQUE_CLIENT_TAG value for the given sync cache_guid.
std::string DeviceInfoLookupString(const std::string& cache_guid) {
  return base::StringPrintf("DeviceInfo_%s", cache_guid.c_str());
}

}  // namespace

SyncedDeviceTracker::SyncedDeviceTracker(syncer::UserShare* user_share,
                                         const std::string& cache_guid)
  : ChangeProcessor(NULL),
    user_share_(user_share),
    cache_guid_(cache_guid),
    local_device_info_tag_(DeviceInfoLookupString(cache_guid)),
    weak_factory_(this) {
  observers_ = new ObserverListThreadSafe<Observer>;
}

SyncedDeviceTracker::~SyncedDeviceTracker() {
}

void SyncedDeviceTracker::StartImpl() { }

void SyncedDeviceTracker::ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) {
  // If desired, we could maintain a cache of device info.  This method will be
  // called with a transaction every time the device info is modified, so this
  // would be the right place to update the cache.
}

void SyncedDeviceTracker::CommitChangesFromSyncModel() {
  observers_->Notify(&Observer::OnDeviceInfoChange);
}

scoped_ptr<DeviceInfo> SyncedDeviceTracker::ReadLocalDeviceInfo() const {
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  return ReadLocalDeviceInfo(trans);
}

scoped_ptr<DeviceInfo> SyncedDeviceTracker::ReadLocalDeviceInfo(
    const syncer::BaseTransaction& trans) const {
  syncer::ReadNode node(&trans);
  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, local_device_info_tag_) !=
      syncer::BaseNode::INIT_OK) {
    return scoped_ptr<DeviceInfo>();
  }

  const sync_pb::DeviceInfoSpecifics& specifics = node.GetDeviceInfoSpecifics();
  return scoped_ptr<DeviceInfo>(
      new DeviceInfo(specifics.cache_guid(),
                     specifics.client_name(),
                     specifics.chrome_version(),
                     specifics.sync_user_agent(),
                     specifics.device_type(),
                     specifics.signin_scoped_device_id()));
}

scoped_ptr<DeviceInfo> SyncedDeviceTracker::ReadDeviceInfo(
    const std::string& client_id) const {
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  syncer::ReadNode node(&trans);
  std::string lookup_string = DeviceInfoLookupString(client_id);
  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, lookup_string) !=
      syncer::BaseNode::INIT_OK) {
    return scoped_ptr<DeviceInfo>();
  }

  const sync_pb::DeviceInfoSpecifics& specifics = node.GetDeviceInfoSpecifics();
  return scoped_ptr<DeviceInfo>(
      new DeviceInfo(specifics.cache_guid(),
                     specifics.client_name(),
                     specifics.chrome_version(),
                     specifics.sync_user_agent(),
                     specifics.device_type(),
                     specifics.signin_scoped_device_id()));
}

void SyncedDeviceTracker::GetAllSyncedDeviceInfo(
    ScopedVector<DeviceInfo>* device_info) const {
  if (device_info == NULL)
    return;

  device_info->clear();

  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  syncer::ReadNode root_node(&trans);

  if (root_node.InitTypeRoot(syncer::DEVICE_INFO) !=
      syncer::BaseNode::INIT_OK) {
    return;
  }

  // Get all the children of the root node and use the child id to read
  // device info for devices.
  std::vector<int64> children;
  root_node.GetChildIds(&children);

  for (std::vector<int64>::const_iterator it = children.begin();
       it != children.end(); ++it) {
    syncer::ReadNode node(&trans);
    if (node.InitByIdLookup(*it) != syncer::BaseNode::INIT_OK)
      return;

    const sync_pb::DeviceInfoSpecifics& specifics =
        node.GetDeviceInfoSpecifics();
    device_info->push_back(new DeviceInfo(specifics.cache_guid(),
                                          specifics.client_name(),
                                          specifics.chrome_version(),
                                          specifics.sync_user_agent(),
                                          specifics.device_type(),
                                          specifics.signin_scoped_device_id()));
  }
}

std::string SyncedDeviceTracker::cache_guid() const {
  return cache_guid_;
}

void SyncedDeviceTracker::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void SyncedDeviceTracker::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void SyncedDeviceTracker::InitLocalDeviceInfo(
    const std::string& signin_scoped_device_id,
    const base::Closure& callback) {
  DeviceInfo::CreateLocalDeviceInfo(
      cache_guid_,
      signin_scoped_device_id,
      base::Bind(&SyncedDeviceTracker::InitLocalDeviceInfoContinuation,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void SyncedDeviceTracker::InitLocalDeviceInfoContinuation(
    const base::Closure& callback, const DeviceInfo& local_info) {
  WriteLocalDeviceInfo(local_info);
  callback.Run();
}

void SyncedDeviceTracker::WriteLocalDeviceInfo(const DeviceInfo& info) {
  DCHECK_EQ(cache_guid_, info.guid());
  WriteDeviceInfo(info, local_device_info_tag_);
}

void SyncedDeviceTracker::WriteDeviceInfo(const DeviceInfo& info,
                                          const std::string& tag) {
  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  syncer::WriteNode node(&trans);

  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(info.guid());
  specifics.set_client_name(info.client_name());
  specifics.set_chrome_version(info.chrome_version());
  specifics.set_sync_user_agent(info.sync_user_agent());
  specifics.set_device_type(info.device_type());
  specifics.set_signin_scoped_device_id(info.signin_scoped_device_id());

  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, tag) ==
      syncer::BaseNode::INIT_OK) {
    const sync_pb::DeviceInfoSpecifics& sync_specifics =
        node.GetDeviceInfoSpecifics();
    if (sync_specifics.has_backup_timestamp())
      specifics.set_backup_timestamp(sync_specifics.backup_timestamp());
    node.SetDeviceInfoSpecifics(specifics);
    node.SetTitle(specifics.client_name());
  } else {
    syncer::ReadNode type_root(&trans);
    syncer::BaseNode::InitByLookupResult type_root_lookup_result =
        type_root.InitTypeRoot(syncer::DEVICE_INFO);
    DCHECK_EQ(syncer::BaseNode::INIT_OK, type_root_lookup_result);

    syncer::WriteNode new_node(&trans);
    syncer::WriteNode::InitUniqueByCreationResult create_result =
        new_node.InitUniqueByCreation(syncer::DEVICE_INFO,
                                      type_root,
                                      tag);
    DCHECK_EQ(syncer::WriteNode::INIT_SUCCESS, create_result);
    new_node.SetDeviceInfoSpecifics(specifics);
    new_node.SetTitle(specifics.client_name());
  }
}

void SyncedDeviceTracker::UpdateLocalDeviceBackupTime(base::Time backup_time) {
  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  syncer::WriteNode node(&trans);

  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, local_device_info_tag_)
          == syncer::BaseNode::INIT_OK) {
    sync_pb::DeviceInfoSpecifics specifics = node.GetDeviceInfoSpecifics();
    int64 new_backup_timestamp = syncer::TimeToProtoTime(backup_time);
    if (!specifics.has_backup_timestamp() ||
        specifics.backup_timestamp() != new_backup_timestamp) {
      specifics.set_backup_timestamp(new_backup_timestamp);
      node.SetDeviceInfoSpecifics(specifics);
    }
  }
}

base::Time SyncedDeviceTracker::GetLocalDeviceBackupTime() const {
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  syncer::ReadNode node(&trans);
  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, local_device_info_tag_)
          == syncer::BaseNode::INIT_OK &&
      node.GetDeviceInfoSpecifics().has_backup_timestamp()) {
    return syncer::ProtoTimeToTime(
        node.GetDeviceInfoSpecifics().backup_timestamp());
  } else {
    return base::Time();
  }
}

}  // namespace browser_sync
