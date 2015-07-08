// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync_bundle.h"

#include "base/location.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

namespace extensions {

SyncBundle::SyncBundle(ExtensionSyncService* sync_service)
    : sync_service_(sync_service) {}

SyncBundle::~SyncBundle() {}

void SyncBundle::MergeDataAndStartSyncing(
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor) {
  sync_processor_.reset(sync_processor.release());

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    scoped_ptr<ExtensionSyncData> extension_sync_data(
        ExtensionSyncData::CreateFromSyncData(sync_data));
    if (extension_sync_data.get()) {
      AddSyncedExtension(extension_sync_data->id());
      sync_service_->ApplySyncData(*extension_sync_data);
    }
  }
}

void SyncBundle::Reset() {
  sync_processor_.reset();
  synced_extensions_.clear();
  pending_sync_data_.clear();
}

bool SyncBundle::IsSyncing() const {
  return sync_processor_ != nullptr;
}

bool SyncBundle::HasExtensionId(const std::string& id) const {
  return synced_extensions_.find(id) != synced_extensions_.end();
}

bool SyncBundle::ShouldIncludeInLocalSyncDataList(
    const Extension& extension) const {
  // If there is pending data for this extension, then this version is out of
  // date. We'll sync back the version we got from sync.
  return IsSyncing() && !HasPendingExtensionId(extension.id());
}

void SyncBundle::PushSyncDataList(
    const syncer::SyncDataList& sync_data_list) {
  syncer::SyncChangeList sync_change_list;
  for (const syncer::SyncData& sync_data : sync_data_list) {
    const syncer::SyncDataLocal sync_data_local(sync_data);
    const std::string& extension_id = sync_data_local.GetTag();

    sync_change_list.push_back(CreateSyncChange(extension_id, sync_data));

    AddSyncedExtension(extension_id);
  }

  PushSyncChanges(sync_change_list);
}

void SyncBundle::PushSyncDeletion(const std::string& extension_id,
                                  const syncer::SyncData& sync_data) {
  RemoveSyncedExtension(extension_id);
  PushSyncChanges(syncer::SyncChangeList(1,
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_DELETE,
                         sync_data)));
}

void SyncBundle::PushSyncChangeIfNeeded(const Extension& extension) {
  syncer::SyncChangeList sync_change_list(
      1,
      CreateSyncChange(extension.id(),
                       sync_service_->CreateSyncData(extension).GetSyncData()));
  PushSyncChanges(sync_change_list);
  MarkPendingExtensionSynced(extension.id());
}

void SyncBundle::ApplySyncChange(const syncer::SyncChange& sync_change) {
  scoped_ptr<ExtensionSyncData> extension_sync_data(
      ExtensionSyncData::CreateFromSyncChange(sync_change));
  if (!extension_sync_data.get())
    return;  // TODO(treib,kalman): Warning message?

  if (extension_sync_data->uninstalled())
    RemoveSyncedExtension(extension_sync_data->id());
  else
    AddSyncedExtension(extension_sync_data->id());
  sync_service_->ApplySyncData(*extension_sync_data);
}

bool SyncBundle::HasPendingExtensionId(const std::string& id) const {
  return pending_sync_data_.find(id) != pending_sync_data_.end();
}

void SyncBundle::AddPendingExtension(
    const std::string& id,
    const ExtensionSyncData& extension_sync_data) {
  pending_sync_data_.insert(std::make_pair(id, extension_sync_data));
}

std::vector<ExtensionSyncData> SyncBundle::GetPendingData() const {
  std::vector<ExtensionSyncData> pending_extensions;
  for (const auto& data : pending_sync_data_)
    pending_extensions.push_back(data.second);

  return pending_extensions;
}

syncer::SyncChange SyncBundle::CreateSyncChange(
    const std::string& extension_id,
    const syncer::SyncData& sync_data) const {
  return syncer::SyncChange(
      FROM_HERE,
      HasExtensionId(extension_id) ? syncer::SyncChange::ACTION_UPDATE
                                   : syncer::SyncChange::ACTION_ADD,
      sync_data);
}

void SyncBundle::PushSyncChanges(
    const syncer::SyncChangeList& sync_change_list) {
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
}

void SyncBundle::AddSyncedExtension(const std::string& id) {
  synced_extensions_.insert(id);
}

void SyncBundle::RemoveSyncedExtension(const std::string& id) {
  synced_extensions_.erase(id);
}

void SyncBundle::MarkPendingExtensionSynced(const std::string& id) {
  pending_sync_data_.erase(id);
  AddSyncedExtension(id);
}

}  // namespace extensions
