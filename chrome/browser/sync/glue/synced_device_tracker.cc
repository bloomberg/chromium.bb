// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_device_tracker.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"

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
    ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
    user_share_(user_share),
    cache_guid_(cache_guid),
    local_device_info_tag_(DeviceInfoLookupString(cache_guid)) {
}

SyncedDeviceTracker::~SyncedDeviceTracker() {
}

void SyncedDeviceTracker::StartImpl(Profile* profile) { }

void SyncedDeviceTracker::ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) {
  // If desired, we could maintain a cache of device info.  This method will be
  // called with a transaction every time the device info is modified, so this
  // would be the right place to update the cache.
}

void SyncedDeviceTracker::CommitChangesFromSyncModel() {
  // TODO(sync): notify our listeners.
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
  return scoped_ptr<DeviceInfo> (
      new DeviceInfo(specifics.client_name(),
                     specifics.chrome_version(),
                     specifics.sync_user_agent(),
                     specifics.device_type()));
}

void SyncedDeviceTracker::InitLocalDeviceInfo(const base::Closure& callback) {
  DeviceInfo::CreateLocalDeviceInfo(
      base::Bind(&SyncedDeviceTracker::InitLocalDeviceInfoContinuation,
                 weak_factory_.GetWeakPtr(), callback));
}

void SyncedDeviceTracker::InitLocalDeviceInfoContinuation(
    const base::Closure& callback, const DeviceInfo& local_info) {
  WriteLocalDeviceInfo(local_info);
  callback.Run();
}

void SyncedDeviceTracker::WriteLocalDeviceInfo(const DeviceInfo& info) {
  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(cache_guid_);
  specifics.set_client_name(info.client_name());
  specifics.set_chrome_version(info.chrome_version());
  specifics.set_sync_user_agent(info.sync_user_agent());
  specifics.set_device_type(info.device_type());

  syncer::WriteTransaction trans(FROM_HERE, user_share_);
  syncer::WriteNode node(&trans);

  if (node.InitByClientTagLookup(syncer::DEVICE_INFO, local_device_info_tag_) ==
      syncer::BaseNode::INIT_OK) {
    node.SetDeviceInfoSpecifics(specifics);
    node.SetTitle(ASCIIToWide(specifics.client_name()));
  } else {
    syncer::ReadNode type_root(&trans);
    syncer::BaseNode::InitByLookupResult type_root_lookup_result =
        type_root.InitByTagLookup(ModelTypeToRootTag(syncer::DEVICE_INFO));
    DCHECK_EQ(syncer::BaseNode::INIT_OK, type_root_lookup_result);

    syncer::WriteNode new_node(&trans);
    syncer::WriteNode::InitUniqueByCreationResult create_result =
        new_node.InitUniqueByCreation(syncer::DEVICE_INFO,
                                      type_root,
                                      local_device_info_tag_);
    DCHECK_EQ(syncer::WriteNode::INIT_SUCCESS, create_result);
    new_node.SetDeviceInfoSpecifics(specifics);
    new_node.SetTitle(ASCIIToWide(specifics.client_name()));
  }
}

}  // namespace browser_sync
