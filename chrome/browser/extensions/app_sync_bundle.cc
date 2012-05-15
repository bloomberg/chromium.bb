// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_sync_bundle.h"

#include "base/location.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"

namespace extensions {

AppSyncBundle::AppSyncBundle(ExtensionService* extension_service)
    : extension_service_(extension_service),
      sync_processor_(NULL) {}

AppSyncBundle::~AppSyncBundle() {}

void AppSyncBundle::SetupSync(SyncChangeProcessor* sync_change_processor,
                              SyncErrorFactory* sync_error_factory,
                              const SyncDataList& initial_sync_data) {
  sync_processor_.reset(sync_change_processor);
  sync_error_factory_.reset(sync_error_factory);

  for (SyncDataList::const_iterator i = initial_sync_data.begin();
       i != initial_sync_data.end();
       ++i) {
    AppSyncData app_sync_data(*i);
    AddApp(app_sync_data.id());
    extension_service_->ProcessAppSyncData(app_sync_data);
  }
}

void AppSyncBundle::Reset() {
  sync_processor_.reset();
  sync_error_factory_.reset();
  synced_apps_.clear();
  pending_sync_data_.clear();
}

SyncChange AppSyncBundle::CreateSyncChangeToDelete(const Extension* extension)
    const {
  AppSyncData sync_data = extension_service_->GetAppSyncData(*extension);
  return sync_data.GetSyncChange(SyncChange::ACTION_DELETE);
}

void AppSyncBundle::ProcessDeletion(std::string extension_id,
                                    const SyncChange& sync_change) {
  RemoveApp(extension_id);
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      SyncChangeList(1, sync_change));
}

SyncChange AppSyncBundle::CreateSyncChange(const SyncData& sync_data) {
  if (HasExtensionId(sync_data.GetTag())) {
    return SyncChange(SyncChange::ACTION_UPDATE, sync_data);
  } else {
    AddApp(sync_data.GetTag());
    return SyncChange(SyncChange::ACTION_ADD, sync_data);
  }
}

SyncDataList AppSyncBundle::GetAllSyncData() const {
  std::vector<AppSyncData> app_sync_data =
      extension_service_->GetAppSyncDataList();
  SyncDataList result(app_sync_data.size());
  for (int i = 0; i < static_cast<int>(app_sync_data.size()); ++i) {
    result[i] = app_sync_data[i].GetSyncData();
  }
  return result;
}

void AppSyncBundle::SyncChangeIfNeeded(const Extension& extension) {
  AppSyncData app_sync_data = extension_service_->GetAppSyncData(extension);

  SyncChangeList sync_change_list(1, app_sync_data.GetSyncChange(
      HasExtensionId(extension.id()) ?
      SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD));
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  MarkPendingAppSynced(extension.id());
}

void AppSyncBundle::ProcessSyncChange(AppSyncData app_sync_data) {
  if (app_sync_data.uninstalled())
    RemoveApp(app_sync_data.id());
  else
    AddApp(app_sync_data.id());
  extension_service_->ProcessAppSyncData(app_sync_data);
}

void AppSyncBundle::ProcessSyncChangeList(SyncChangeList sync_change_list) {
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  extension_service_->extension_prefs()->extension_sorting()->
      FixNTPOrdinalCollisions();
}

bool AppSyncBundle::HasExtensionId(const std::string& id) const {
  return synced_apps_.find(id) != synced_apps_.end();
}

bool AppSyncBundle::HasPendingExtensionId(const std::string& id) const {
  return pending_sync_data_.find(id) != pending_sync_data_.end();
}

void AppSyncBundle::AddPendingApp(const std::string& id,
                                  const AppSyncData& app_sync_data) {
  pending_sync_data_[id] = app_sync_data;
}

bool AppSyncBundle::HandlesApp(const Extension& extension) const {
  return sync_processor_ != NULL &&
      extension.GetSyncType() == Extension::SYNC_TYPE_APP;
}

std::vector<AppSyncData> AppSyncBundle::GetPendingData() const {
  std::vector<AppSyncData> pending_apps;
  for (std::map<std::string, AppSyncData>::const_iterator
           i = pending_sync_data_.begin();
       i != pending_sync_data_.end();
       ++i) {
    pending_apps.push_back(i->second);
  }

  return pending_apps;
}

void AppSyncBundle::GetAppSyncDataListHelper(
    const ExtensionSet& extensions,
    std::vector<AppSyncData>* sync_data_list) const {
  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension& extension = **it;
    // If we have pending app data for this app, then this
    // version is out of date.  We'll sync back the version we got from
    // sync.
    if (HandlesApp(extension) &&
        !HasPendingExtensionId(extension.id())) {
      sync_data_list->push_back(extension_service_->GetAppSyncData(extension));
    }
  }
}

void AppSyncBundle::AddApp(const std::string& id) {
  synced_apps_.insert(id);
}

void AppSyncBundle::RemoveApp(const std::string& id) {
  synced_apps_.erase(id);
}


void AppSyncBundle::MarkPendingAppSynced(const std::string& id) {
  pending_sync_data_.erase(id);
  synced_apps_.insert(id);
}


}  // namespace extensions
