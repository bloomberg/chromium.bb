// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_bundle.h"

#include "base/location.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"

namespace extensions {

ExtensionSyncBundle::ExtensionSyncBundle(
    ExtensionSyncService* extension_sync_service)
    : extension_sync_service_(extension_sync_service) {}

ExtensionSyncBundle::~ExtensionSyncBundle() {}

void ExtensionSyncBundle::SetupSync(
    syncer::SyncChangeProcessor* sync_processor,
    syncer::SyncErrorFactory* sync_error_factory,
    const syncer::SyncDataList& initial_sync_data) {
  sync_processor_.reset(sync_processor);
  sync_error_factory_.reset(sync_error_factory);

  for (syncer::SyncDataList::const_iterator i = initial_sync_data.begin();
       i != initial_sync_data.end();
       ++i) {
    ExtensionSyncData extension_sync_data(*i);
    AddExtension(extension_sync_data.id());
    extension_sync_service_->ProcessExtensionSyncData(extension_sync_data);
  }
}

void ExtensionSyncBundle::Reset() {
  sync_processor_.reset();
  sync_error_factory_.reset();
  synced_extensions_.clear();
  pending_sync_data_.clear();
}

syncer::SyncChange ExtensionSyncBundle::CreateSyncChangeToDelete(
    const Extension* extension) const {
  extensions::ExtensionSyncData sync_data =
      extension_sync_service_->GetExtensionSyncData(*extension);
  return sync_data.GetSyncChange(syncer::SyncChange::ACTION_DELETE);
}

void ExtensionSyncBundle::ProcessDeletion(
    std::string extension_id, const syncer::SyncChange& sync_change) {
  RemoveExtension(extension_id);
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      syncer::SyncChangeList(1, sync_change));
}

syncer::SyncChange ExtensionSyncBundle::CreateSyncChange(
    const syncer::SyncData& sync_data) {
  const syncer::SyncDataLocal sync_data_local(sync_data);
  if (HasExtensionId(sync_data_local.GetTag())) {
    return syncer::SyncChange(FROM_HERE,
                              syncer::SyncChange::ACTION_UPDATE,
                              sync_data);
  } else {
    AddExtension(sync_data_local.GetTag());
    return syncer::SyncChange(FROM_HERE,
                              syncer::SyncChange::ACTION_ADD,
                              sync_data);
  }
}

syncer::SyncDataList ExtensionSyncBundle::GetAllSyncData() const {
  std::vector<ExtensionSyncData> extension_sync_data =
      extension_sync_service_->GetExtensionSyncDataList();
  syncer::SyncDataList result(extension_sync_data.size());
  for (int i = 0; i < static_cast<int>(extension_sync_data.size()); ++i) {
    result[i] = extension_sync_data[i].GetSyncData();
  }
  return result;
}

void ExtensionSyncBundle::ProcessSyncChange(
    ExtensionSyncData extension_sync_data) {
  if (extension_sync_data.uninstalled())
    RemoveExtension(extension_sync_data.id());
  else
    AddExtension(extension_sync_data.id());
  extension_sync_service_->ProcessExtensionSyncData(extension_sync_data);
}

void ExtensionSyncBundle::ProcessSyncChangeList(
    syncer::SyncChangeList sync_change_list) {
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
}

bool ExtensionSyncBundle::HasExtensionId(
    const std::string& id) const {
  return synced_extensions_.find(id) != synced_extensions_.end();
}

bool ExtensionSyncBundle::HasPendingExtensionId(
    const std::string& id) const {
  return pending_sync_data_.find(id) != pending_sync_data_.end();
}

void ExtensionSyncBundle::AddPendingExtension(
    const std::string& id,
    const ExtensionSyncData& extension_sync_data) {
  pending_sync_data_[id] = extension_sync_data;
}

bool ExtensionSyncBundle::IsSyncing() const {
  return sync_processor_ != NULL;
}

void ExtensionSyncBundle::SyncChangeIfNeeded(const Extension& extension) {
  ExtensionSyncData extension_sync_data =
      extension_sync_service_->GetExtensionSyncData(extension);

  syncer::SyncChangeList sync_change_list(1, extension_sync_data.GetSyncChange(
      HasExtensionId(extension.id()) ?
      syncer::SyncChange::ACTION_UPDATE : syncer::SyncChange::ACTION_ADD));
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  MarkPendingExtensionSynced(extension.id());
}

std::vector<ExtensionSyncData> ExtensionSyncBundle::GetPendingData() const {
  std::vector<ExtensionSyncData> pending_extensions;
  for (std::map<std::string, ExtensionSyncData>::const_iterator
           i = pending_sync_data_.begin();
       i != pending_sync_data_.end();
       ++i) {
    pending_extensions.push_back(i->second);
  }

  return pending_extensions;
}

void ExtensionSyncBundle::GetExtensionSyncDataListHelper(
    const ExtensionSet& extensions,
    std::vector<ExtensionSyncData>* sync_data_list) const {
  Profile* profile = extension_sync_service_->profile();

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension& extension = *it->get();
    // If we have pending extension data for this extension, then this
    // version is out of date.  We'll sync back the version we got from
    // sync.
    if (IsSyncing() && util::ShouldSyncExtension(&extension, profile) &&
        !HasPendingExtensionId(extension.id())) {
      sync_data_list->push_back(
          extension_sync_service_->GetExtensionSyncData(extension));
    }
  }
}

void ExtensionSyncBundle::AddExtension(const std::string& id) {
  synced_extensions_.insert(id);
}

void ExtensionSyncBundle::RemoveExtension(const std::string& id) {
  synced_extensions_.erase(id);
}

void ExtensionSyncBundle::MarkPendingExtensionSynced(const std::string& id) {
  pending_sync_data_.erase(id);
  synced_extensions_.insert(id);
}

}  // namespace extensions
