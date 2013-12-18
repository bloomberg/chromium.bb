// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_source.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model.h"

using syncer::SyncChange;

namespace app_list {

namespace {

void UpdateSyncItemFromSync(const sync_pb::AppListSpecifics& specifics,
                            AppListSyncableService::SyncItem* item) {
  DCHECK_EQ(item->item_id, specifics.item_id());
  item->item_type = specifics.item_type();
  item->item_name = specifics.item_name();
  item->parent_id = specifics.parent_id();
  if (!specifics.page_ordinal().empty())
    item->page_ordinal = syncer::StringOrdinal(specifics.page_ordinal());
  if (!specifics.item_ordinal().empty())
    item->item_ordinal = syncer::StringOrdinal(specifics.item_ordinal());
}

bool UpdateSyncItemFromAppItem(const AppListItemModel* app_item,
                               AppListSyncableService::SyncItem* sync_item) {
  DCHECK_EQ(sync_item->item_id, app_item->id());
  bool changed = false;
  if (!sync_item->item_ordinal.IsValid() ||
      !app_item->position().Equals(sync_item->item_ordinal)) {
    sync_item->item_ordinal = app_item->position();
    changed = true;
  }
  // TODO(stevenjb): Set parent_id and page_ordinal.
  return changed;
}

void GetSyncSpecificsFromSyncItem(const AppListSyncableService::SyncItem* item,
                                  sync_pb::AppListSpecifics* specifics) {
  DCHECK(specifics);
  specifics->set_item_id(item->item_id);
  specifics->set_item_type(item->item_type);
  specifics->set_item_name(item->item_name);
  specifics->set_parent_id(item->parent_id);
  if (item->page_ordinal.IsValid())
    specifics->set_page_ordinal(item->page_ordinal.ToInternalValue());
  if (item->item_ordinal.IsValid())
    specifics->set_item_ordinal(item->item_ordinal.ToInternalValue());
}

syncer::SyncData GetSyncDataFromSyncItem(
    const AppListSyncableService::SyncItem* item) {
  sync_pb::EntitySpecifics specifics;
  GetSyncSpecificsFromSyncItem(item, specifics.mutable_app_list());
  return syncer::SyncData::CreateLocalData(item->item_id,
                                           item->item_id,
                                           specifics);
}

}  // namespace

// AppListSyncableService::SyncItem

AppListSyncableService::SyncItem::SyncItem(
    const std::string& id,
    sync_pb::AppListSpecifics::AppListItemType type)
    : item_id(id),
      item_type(type) {
}

AppListSyncableService::SyncItem::~SyncItem() {
}

// AppListSyncableService

AppListSyncableService::AppListSyncableService(
    Profile* profile,
    ExtensionService* extension_service)
    : profile_(profile),
      model_(new AppListModel) {
  if (extension_service && extension_service->is_ready()) {
    BuildModel();
    return;
  }

  // The extensions for this profile have not yet all been loaded.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile));
}

AppListSyncableService::~AppListSyncableService() {
  STLDeleteContainerPairSecondPointers(sync_items_.begin(), sync_items_.end());
}

void AppListSyncableService::BuildModel() {
  // For now, use the AppListControllerDelegate associated with the native
  // desktop. TODO(stevenjb): Remove ExtensionAppModelBuilder controller
  // dependency and move the dependent methods from AppListControllerDelegate
  // to an extension service delegate associated with this class.
  AppListControllerDelegate* controller = NULL;
  AppListService* service =
      AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (service)
    controller = service->GetControllerDelegate();
  apps_builder_.reset(new ExtensionAppModelBuilder(controller));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSyncAppList)) {
    DVLOG(1) << this << ": AppListSyncableService: InitializeWithService.";
    SyncStarted();
    apps_builder_->InitializeWithService(this);
  } else {
    DVLOG(1) << this << ": AppListSyncableService: InitializeWithProfile.";
    apps_builder_->InitializeWithProfile(profile_, model_.get());
  }

  DCHECK(profile_);
}

void AppListSyncableService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSIONS_READY, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  registrar_.RemoveAll();
  BuildModel();
}

const AppListSyncableService::SyncItem*
AppListSyncableService::GetSyncItem(const std::string& id) const {
  SyncItemMap::const_iterator iter = sync_items_.find(id);
  if (iter != sync_items_.end())
    return iter->second;
  return NULL;
}

void AppListSyncableService::AddExtensionAppItem(ExtensionAppItem* item) {
  SyncItem* sync_item = AddItem(
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, item);
  if (!sync_item)
    return;  // Item already exists.
  sync_item->item_name = item->extension_name();
  SendSyncChange(sync_item, SyncChange::ACTION_ADD);
}

void AppListSyncableService::UpdateExtensionAppItem(ExtensionAppItem* item) {
  SyncItem* sync_item = FindSyncItem(item->id());
  if (!sync_item) {
    LOG(ERROR) << "UpdateExtensionAppItem: no sync item: " << item->id();
    return;
  }
  bool changed = UpdateSyncItemFromAppItem(item, sync_item);
  if (sync_item->item_name != item->extension_name()) {
    sync_item->item_name = item->extension_name();
    changed = true;
  }
  if (!changed) {
    DVLOG(2) << this << " - Update: SYNC NO CHANGE: " << sync_item->ToString();
    return;
  }
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
}

void AppListSyncableService::RemoveItem(const std::string& id) {
  DVLOG(2) << this << ": RemoveItem: " << id.substr(0, 8);
  SyncItemMap::iterator iter = sync_items_.find(id);
  if (iter == sync_items_.end())
    return;
  SyncItem* sync_item = iter->second;
  if (SyncStarted()) {
    DVLOG(2) << this << " -> SYNC DELETE: " << sync_item->ToString();
    SyncChange sync_change(FROM_HERE, SyncChange::ACTION_DELETE,
                           GetSyncDataFromSyncItem(sync_item));
    sync_processor_->ProcessSyncChanges(
        FROM_HERE, syncer::SyncChangeList(1, sync_change));
  }
  delete sync_item;
  sync_items_.erase(iter);
  model_->item_list()->DeleteItem(id);
}

// AppListSyncableService syncer::SyncableService

syncer::SyncMergeResult AppListSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());

  sync_processor_ = sync_processor.Pass();
  sync_error_handler_ = error_handler.Pass();

  syncer::SyncMergeResult result = syncer::SyncMergeResult(type);
  result.set_num_items_before_association(sync_items_.size());
  DVLOG(1) << this << ": MergeDataAndStartSyncing: "
           << initial_sync_data.size();

  // Copy all sync items to |unsynced_items|.
  std::set<std::string> unsynced_items;
  for (SyncItemMap::const_iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ++iter) {
    unsynced_items.insert(iter->first);
  }

  // Create SyncItem entries for initial_sync_data.
  size_t new_items = 0, updated_items = 0;
  for (syncer::SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end(); ++iter) {
    const syncer::SyncData& data = *iter;
    DCHECK_EQ(syncer::APP_LIST, data.GetDataType());
    if (CreateOrUpdateSyncItem(data.GetSpecifics().app_list()))
      ++new_items;
    else
      ++updated_items;
    unsynced_items.erase(data.GetSpecifics().app_list().item_id());
  }

  result.set_num_items_after_association(sync_items_.size());
  result.set_num_items_added(new_items);
  result.set_num_items_deleted(0);
  result.set_num_items_modified(updated_items);

  // Send unsynced items. Does not affect |result|.
  syncer::SyncChangeList change_list;
  for (std::set<std::string>::iterator iter = unsynced_items.begin();
       iter != unsynced_items.end(); ++iter) {
    SyncItem* sync_item = FindSyncItem(*iter);
    DVLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
    change_list.push_back(SyncChange(FROM_HERE,  SyncChange::ACTION_ADD,
                                     GetSyncDataFromSyncItem(sync_item)));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);

  return result;
}

void AppListSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::APP_LIST);

  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList AppListSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::APP_LIST, type);

  DVLOG(1) << this << "GetAllSyncData: " << sync_items_.size();
  syncer::SyncDataList list;
  for (SyncItemMap::const_iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ++iter) {
    DVLOG(2) << this << " -> SYNC: " << iter->second->ToString();
    list.push_back(GetSyncDataFromSyncItem(iter->second));
  }
  return list;
}

syncer::SyncError AppListSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    return syncer::SyncError(FROM_HERE,
                             syncer::SyncError::DATATYPE_ERROR,
                             "App List syncable service is not started.",
                             syncer::APP_LIST);
  }

  // Process incoming changes first.
  DVLOG(1) << this << ": ProcessSyncChanges: " << change_list.size();
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    const SyncChange& change = *iter;
    if (change.change_type() == SyncChange::ACTION_ADD ||
        change.change_type() == SyncChange::ACTION_UPDATE) {
      CreateOrUpdateSyncItem(change.sync_data().GetSpecifics().app_list());
    } else if (change.change_type() == SyncChange::ACTION_DELETE) {
      DeleteSyncItem(change.sync_data().GetSpecifics().app_list());
    } else {
      LOG(ERROR) << "Invalid sync change";
    }
  }
  return syncer::SyncError();
}

// AppListSyncableService private

void AppListSyncableService::CreateAppItemFromSyncItem(SyncItem* sync_item) {
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_APP) {
    std::string extension_id = sync_item->item_id;
    const ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    const extensions::Extension* app =
        extension_service->GetInstalledExtension(extension_id);
    DVLOG_IF(1, !app) << this << "No App for ID: " << extension_id;
    bool is_platform_app = app ? app->is_platform_app() : false;
    ExtensionAppItem* app_item = new ExtensionAppItem(
        profile_,
        sync_item,
        extension_id,
        sync_item->item_name,
        gfx::ImageSkia(),
        is_platform_app);
    model_->item_list()->AddItem(app_item);
    return;
  }
  if (sync_item->item_type ==
      sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    // TODO(stevenjb): Implement
  }
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_FOLDER) {
    // TODO(stevenjb): Implement
  }
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_URL) {
    // TODO(stevenjb): Implement
  }
  LOG(ERROR) << "Unsupported type: " << sync_item->item_type;
}

bool AppListSyncableService::SyncStarted() {
  if (sync_processor_.get())
    return true;
  if (flare_.is_null()) {
    DVLOG(2) << this << ": SyncStarted: Flare.";
    flare_ = sync_start_util::GetFlareForSyncableService(profile_->GetPath());
    flare_.Run(syncer::APP_LIST);
  }
  return false;
}

AppListSyncableService::SyncItem* AppListSyncableService::AddItem(
    sync_pb::AppListSpecifics::AppListItemType type,
    AppListItemModel* app_item) {
  const std::string& item_id = app_item->id();
  if (item_id.empty()) {
    LOG(ERROR) << "AppListItemModel item with empty ID";
    return NULL;
  }
  bool new_item = false;
  SyncItem* sync_item = FindOrCreateSyncItem(item_id, type, &new_item);
  if (!new_item) {
    DVLOG(2) << this << ": AddItem already exists: " << sync_item->ToString();
    return NULL;  // Item already exists.
  }
  UpdateSyncItemFromAppItem(app_item, sync_item);
  DVLOG(1) << this << ": AddItem: " << sync_item->ToString();
  model_->item_list()->AddItem(app_item);
  return sync_item;
}

void AppListSyncableService::SendSyncChange(
    SyncItem* sync_item,
    SyncChange::SyncChangeType sync_change_type) {
  if (!SyncStarted()) {
    DVLOG(2) << this << " - SendSyncChange: SYNC NOT STARTED: "
             << sync_item->ToString();
    return;
  }
  if (sync_change_type == SyncChange::ACTION_ADD)
    DVLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
  else
    DVLOG(2) << this << " -> SYNC UPDATE: " << sync_item->ToString();
  SyncChange sync_change(FROM_HERE, sync_change_type,
                         GetSyncDataFromSyncItem(sync_item));
  sync_processor_->ProcessSyncChanges(
      FROM_HERE, syncer::SyncChangeList(1, sync_change));
}

AppListSyncableService::SyncItem*
AppListSyncableService::FindSyncItem(const std::string& item_id) {
  SyncItemMap::iterator iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return NULL;
  return iter->second;
}

AppListSyncableService::SyncItem* AppListSyncableService::FindOrCreateSyncItem(
    const std::string& item_id,
    sync_pb::AppListSpecifics::AppListItemType type,
    bool* new_item) {
  SyncItem* item = FindSyncItem(item_id);
  if (item) {
    DCHECK(type == item->item_type);
    *new_item = false;
    return item;
  }

  item = new SyncItem(item_id, type);
  sync_items_[item_id] = item;
  *new_item = true;
  return item;
}

bool AppListSyncableService::CreateOrUpdateSyncItem(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "CreateOrUpdate AppList item with empty ID";
    return false;
  }
  bool new_item = false;
  SyncItem* sync_item =
      FindOrCreateSyncItem(item_id, specifics.item_type(), &new_item);
  DVLOG(2) << this << "CreateOrUpdateSyncItem: " << sync_item->ToString()
           << " New: " << new_item << " Pos: " << specifics.item_ordinal();
  UpdateSyncItemFromSync(specifics, sync_item);
  // Update existing item in model
  AppListItemModel* item = model_->item_list()->FindItem(sync_item->item_id);
  if (item && !item->position().Equals(sync_item->item_ordinal))
    model_->item_list()->SetItemPosition(item, sync_item->item_ordinal);
  if (new_item) {
    CreateAppItemFromSyncItem(sync_item);
    DVLOG(2) << this << " <- SYNC ADD: " << sync_item->ToString();
  } else {
    DVLOG(2) << this << " <- SYNC UPDATE: " << sync_item->ToString();
  }
  return new_item;
}

void AppListSyncableService::DeleteSyncItem(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "Delete AppList item with empty ID";
    return;
  }
  DVLOG(2) << this << "DeleteSyncItem: " << item_id.substr(0, 8);
  SyncItemMap::iterator iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return;
  DVLOG(2) << this << " <- SYNC DELETE: " << iter->second->ToString();
  delete iter->second;
  sync_items_.erase(iter);
  model_->item_list()->DeleteItem(item_id);
}

std::string AppListSyncableService::SyncItem::ToString() const {
  return item_id.substr(0, 8) + " [" + item_ordinal.ToDebugString() + "]";
}

}  // namespace app_list
