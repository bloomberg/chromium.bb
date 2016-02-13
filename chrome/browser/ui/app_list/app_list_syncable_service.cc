// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/apps/drive/drive_app_provider.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/model_pref_updater.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/genius_app/app_id.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#endif

using syncer::SyncChange;

namespace app_list {

namespace {

const char kOemFolderId[] = "ddb1da55-d478-4243-8642-56d3041f0263";

// Prefix for a sync id of a Drive app. Drive app ids are in a different
// format and have to be used because a Drive app could have only an URL
// without a matching Chrome app. To differentiate the Drive app id from
// Chrome app ids, this prefix will be added to create the sync item id
// for a Drive app item.
const char kDriveAppSyncIdPrefix[] = "drive-app-";

void UpdateSyncItemFromSync(const sync_pb::AppListSpecifics& specifics,
                            AppListSyncableService::SyncItem* item) {
  DCHECK_EQ(item->item_id, specifics.item_id());
  item->item_type = specifics.item_type();
  item->item_name = specifics.item_name();
  item->parent_id = specifics.parent_id();
  if (!specifics.item_ordinal().empty())
    item->item_ordinal = syncer::StringOrdinal(specifics.item_ordinal());
}

bool UpdateSyncItemFromAppItem(const AppListItem* app_item,
                               AppListSyncableService::SyncItem* sync_item) {
  DCHECK_EQ(sync_item->item_id, app_item->id());
  bool changed = false;
  if (app_list::switches::IsFolderUIEnabled() &&
      sync_item->parent_id != app_item->folder_id()) {
    sync_item->parent_id = app_item->folder_id();
    changed = true;
  }
  if (sync_item->item_name != app_item->name()) {
    sync_item->item_name = app_item->name();
    changed = true;
  }
  if (!sync_item->item_ordinal.IsValid() ||
      !app_item->position().Equals(sync_item->item_ordinal)) {
    sync_item->item_ordinal = app_item->position();
    changed = true;
  }
  return changed;
}

void GetSyncSpecificsFromSyncItem(const AppListSyncableService::SyncItem* item,
                                  sync_pb::AppListSpecifics* specifics) {
  DCHECK(specifics);
  specifics->set_item_id(item->item_id);
  specifics->set_item_type(item->item_type);
  specifics->set_item_name(item->item_name);
  specifics->set_parent_id(item->parent_id);
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

bool AppIsDefault(ExtensionService* service, const std::string& id) {
  return service && extensions::ExtensionPrefs::Get(service->profile())
                        ->WasInstalledByDefault(id);
}

bool IsUnRemovableDefaultApp(const std::string& id) {
  if (id == extension_misc::kChromeAppId ||
      id == extensions::kWebStoreAppId)
    return true;
#if defined(OS_CHROMEOS)
  if (id == file_manager::kFileManagerAppId || id == genius_app::kGeniusAppId)
    return true;
#endif
  return false;
}

void UninstallExtension(ExtensionService* service, const std::string& id) {
  if (service) {
    ExtensionService::UninstallExtensionHelper(
        service, id, extensions::UNINSTALL_REASON_SYNC);
  }
}

bool GetAppListItemType(AppListItem* item,
                        sync_pb::AppListSpecifics::AppListItemType* type) {
  const char* item_type = item->GetItemType();
  if (item_type == ExtensionAppItem::kItemType) {
    *type = sync_pb::AppListSpecifics::TYPE_APP;
#if defined(OS_CHROMEOS)
  } else if (item_type == ArcAppItem::kItemType) {
    *type = sync_pb::AppListSpecifics::TYPE_APP;
#endif
  } else if (item_type == AppListFolderItem::kItemType) {
    *type = sync_pb::AppListSpecifics::TYPE_FOLDER;
  } else {
    LOG(ERROR) << "Unrecognized model type: " << item_type;
    return false;
  }
  return true;
}

bool IsDriveAppSyncId(const std::string& sync_id) {
  return base::StartsWith(sync_id, kDriveAppSyncIdPrefix,
                          base::CompareCase::SENSITIVE);
}

std::string GetDriveAppSyncId(const std::string& drive_app_id) {
  return kDriveAppSyncIdPrefix + drive_app_id;
}

std::string GetDriveAppIdFromSyncId(const std::string& sync_id) {
  if (!IsDriveAppSyncId(sync_id))
    return std::string();
  return sync_id.substr(strlen(kDriveAppSyncIdPrefix));
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

// AppListSyncableService::ModelObserver

class AppListSyncableService::ModelObserver : public AppListModelObserver {
 public:
  explicit ModelObserver(AppListSyncableService* owner)
      : owner_(owner),
        adding_item_(NULL) {
    DVLOG(2) << owner_ << ": ModelObserver Added";
    owner_->GetModel()->AddObserver(this);
  }

  ~ModelObserver() override {
    owner_->GetModel()->RemoveObserver(this);
    DVLOG(2) << owner_ << ": ModelObserver Removed";
  }

 private:
  // AppListModelObserver
  void OnAppListItemAdded(AppListItem* item) override {
    DCHECK(!adding_item_);
    adding_item_ = item;  // Ignore updates while adding an item.
    VLOG(2) << owner_ << " OnAppListItemAdded: " << item->ToDebugString();
    owner_->AddOrUpdateFromSyncItem(item);
    adding_item_ = NULL;
  }

  void OnAppListItemWillBeDeleted(AppListItem* item) override {
    DCHECK(!adding_item_);
    VLOG(2) << owner_ << " OnAppListItemDeleted: " << item->ToDebugString();
    // Don't sync folder removal in case the folder still exists on another
    // device (e.g. with device specific items in it). Empty folders will be
    // deleted when the last item is removed (in PruneEmptySyncFolders()).
    if (item->GetItemType() == AppListFolderItem::kItemType)
      return;
    owner_->RemoveSyncItem(item->id());
  }

  void OnAppListItemUpdated(AppListItem* item) override {
    if (adding_item_) {
      // Adding an item may trigger update notifications which should be
      // ignored.
      DCHECK_EQ(adding_item_, item);
      return;
    }
    VLOG(2) << owner_ << " OnAppListItemUpdated: " << item->ToDebugString();
    owner_->UpdateSyncItem(item);
  }

  AppListSyncableService* owner_;
  AppListItem* adding_item_;  // Unowned pointer to item being added.

  DISALLOW_COPY_AND_ASSIGN(ModelObserver);
};

// AppListSyncableService

AppListSyncableService::AppListSyncableService(
    Profile* profile,
    extensions::ExtensionSystem* extension_system)
    : profile_(profile),
      extension_system_(extension_system),
      model_(new AppListModel),
      initial_sync_data_processed_(false),
      first_app_list_sync_(true) {
  if (!extension_system) {
    LOG(ERROR) << "AppListSyncableService created with no ExtensionSystem";
    return;
  }

  oem_folder_name_ =
      l10n_util::GetStringUTF8(IDS_APP_LIST_OEM_DEFAULT_FOLDER_NAME);
}

AppListSyncableService::~AppListSyncableService() {
  // Remove observers.
  model_observer_.reset();
  model_pref_updater_.reset();

  STLDeleteContainerPairSecondPointers(sync_items_.begin(), sync_items_.end());
}

void AppListSyncableService::BuildModel() {
  // TODO(calamity): make this a DCHECK after a dev channel release.
  CHECK(extension_system_->extension_service() &&
        extension_system_->extension_service()->is_ready());
  AppListControllerDelegate* controller = NULL;
  AppListService* service = AppListService::Get();
  if (service)
    controller = service->GetControllerDelegate();
  apps_builder_.reset(new ExtensionAppModelBuilder(controller));
#if defined(OS_CHROMEOS)
  arc_apps_builder_.reset(new ArcAppModelBuilder(controller));
#endif
  DCHECK(profile_);
  if (app_list::switches::IsAppListSyncEnabled()) {
    VLOG(1) << this << ": AppListSyncableService: InitializeWithService.";
    SyncStarted();
    apps_builder_->InitializeWithService(this, model_.get());
#if defined(OS_CHROMEOS)
    arc_apps_builder_->InitializeWithService(this, model_.get());
#endif
  } else {
    VLOG(1) << this << ": AppListSyncableService: InitializeWithProfile.";
    apps_builder_->InitializeWithProfile(profile_, model_.get());
#if defined(OS_CHROMEOS)
    arc_apps_builder_->InitializeWithProfile(profile_, model_.get());
#endif
  }

  model_pref_updater_.reset(
      new ModelPrefUpdater(AppListPrefs::Get(profile_), model_.get()));

  if (app_list::switches::IsDriveAppsInAppListEnabled())
    drive_app_provider_.reset(new DriveAppProvider(profile_, this));
}

size_t AppListSyncableService::GetNumSyncItemsForTest() {
  // If the model isn't built yet, there will be no sync items.
  GetModel();

  return sync_items_.size();
}

void AppListSyncableService::ResetDriveAppProviderForTest() {
  drive_app_provider_.reset();
}

void AppListSyncableService::Shutdown() {
  // DriveAppProvider touches other KeyedServices in its dtor and needs be
  // released in shutdown stage.
  drive_app_provider_.reset();
}

void AppListSyncableService::TrackUninstalledDriveApp(
    const std::string& drive_app_id) {
  const std::string sync_id = GetDriveAppSyncId(drive_app_id);
  SyncItem* sync_item = FindSyncItem(sync_id);
  if (sync_item) {
    DCHECK_EQ(sync_item->item_type,
              sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);
    return;
  }

  sync_item = CreateSyncItem(
      sync_id, sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);
  SendSyncChange(sync_item, SyncChange::ACTION_ADD);
}

void AppListSyncableService::UntrackUninstalledDriveApp(
    const std::string& drive_app_id) {
  const std::string sync_id = GetDriveAppSyncId(drive_app_id);
  SyncItem* sync_item = FindSyncItem(sync_id);
  if (!sync_item)
    return;

  DCHECK_EQ(sync_item->item_type,
            sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);
  DeleteSyncItem(sync_item);
}

const AppListSyncableService::SyncItem*
AppListSyncableService::GetSyncItem(const std::string& id) const {
  SyncItemMap::const_iterator iter = sync_items_.find(id);
  if (iter != sync_items_.end())
    return iter->second;
  return NULL;
}

void AppListSyncableService::SetOemFolderName(const std::string& name) {
  oem_folder_name_ = name;
  AppListFolderItem* oem_folder = model_->FindFolderItem(kOemFolderId);
  if (oem_folder)
    model_->SetItemName(oem_folder, oem_folder_name_);
}

AppListModel* AppListSyncableService::GetModel() {
  if (!apps_builder_)
    BuildModel();

  return model_.get();
}

void AppListSyncableService::AddItem(scoped_ptr<AppListItem> app_item) {
  SyncItem* sync_item = FindOrAddSyncItem(app_item.get());
  if (!sync_item)
    return;  // Item is not valid.

  std::string folder_id;
  if (app_list::switches::IsFolderUIEnabled()) {
    if (AppIsOem(app_item->id())) {
      folder_id = FindOrCreateOemFolder();
      VLOG_IF(2, !folder_id.empty())
          << this << ": AddItem to OEM folder: " << sync_item->ToString();
    } else {
      folder_id = sync_item->parent_id;
    }
  }
  VLOG(2) << this << ": AddItem: " << sync_item->ToString()
          << " Folder: '" << folder_id << "'";
  model_->AddItemToFolder(std::move(app_item), folder_id);
}

AppListSyncableService::SyncItem* AppListSyncableService::FindOrAddSyncItem(
    AppListItem* app_item) {
  const std::string& item_id = app_item->id();
  if (item_id.empty()) {
    LOG(ERROR) << "AppListItem item with empty ID";
    return NULL;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If there is an existing, non-REMOVE_DEFAULT entry, return it.
    if (sync_item->item_type !=
        sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      DVLOG(2) << this << ": AddItem already exists: " << sync_item->ToString();
      return sync_item;
    }

    if (RemoveDefaultApp(app_item, sync_item))
      return NULL;

    // Fall through. The REMOVE_DEFAULT_APP entry has been deleted, now a new
    // App entry can be added.
  }

  return CreateSyncItemFromAppItem(app_item);
}

AppListSyncableService::SyncItem*
AppListSyncableService::CreateSyncItemFromAppItem(AppListItem* app_item) {
  sync_pb::AppListSpecifics::AppListItemType type;
  if (!GetAppListItemType(app_item, &type))
    return NULL;
  VLOG(2) << this << " CreateSyncItemFromAppItem:" << app_item->ToDebugString();
  SyncItem* sync_item = CreateSyncItem(app_item->id(), type);
  UpdateSyncItemFromAppItem(app_item, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_ADD);
  return sync_item;
}

void AppListSyncableService::AddOrUpdateFromSyncItem(AppListItem* app_item) {
  // Do not create a sync item for the OEM folder here, do that in
  // ResolveFolderPositions once the position has been resolved.
  if (app_item->id() == kOemFolderId)
    return;

  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (sync_item) {
    UpdateAppItemFromSyncItem(sync_item, app_item);
    return;
  }
  CreateSyncItemFromAppItem(app_item);
}

bool AppListSyncableService::RemoveDefaultApp(AppListItem* item,
                                              SyncItem* sync_item) {
  CHECK_EQ(sync_item->item_type,
           sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);

  // If there is an existing REMOVE_DEFAULT_APP entry, and the app is
  // installed as a Default app, uninstall the app instead of adding it.
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_APP &&
      AppIsDefault(extension_system_->extension_service(), item->id())) {
    VLOG(2) << this << ": HandleDefaultApp: Uninstall: "
            << sync_item->ToString();
    UninstallExtension(extension_system_->extension_service(), item->id());
    return true;
  }

  // Otherwise, we are adding the app as a non-default app (i.e. an app that
  // was installed by Default and removed is getting installed explicitly by
  // the user), so delete the REMOVE_DEFAULT_APP.
  DeleteSyncItem(sync_item);
  return false;
}

void AppListSyncableService::DeleteSyncItem(SyncItem* sync_item) {
  if (SyncStarted()) {
    VLOG(2) << this << " -> SYNC DELETE: " << sync_item->ToString();
    SyncChange sync_change(FROM_HERE, SyncChange::ACTION_DELETE,
                           GetSyncDataFromSyncItem(sync_item));
    sync_processor_->ProcessSyncChanges(
        FROM_HERE, syncer::SyncChangeList(1, sync_change));
  }
  std::string item_id = sync_item->item_id;
  delete sync_item;
  sync_items_.erase(item_id);
}

void AppListSyncableService::UpdateSyncItem(AppListItem* app_item) {
  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (!sync_item) {
    LOG(ERROR) << "UpdateItem: no sync item: " << app_item->id();
    return;
  }
  bool changed = UpdateSyncItemFromAppItem(app_item, sync_item);
  if (!changed) {
    DVLOG(2) << this << " - Update: SYNC NO CHANGE: " << sync_item->ToString();
    return;
  }
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
}

void AppListSyncableService::RemoveItem(const std::string& id) {
  RemoveSyncItem(id);
  model_->DeleteItem(id);
  PruneEmptySyncFolders();
}

void AppListSyncableService::RemoveUninstalledItem(const std::string& id) {
  RemoveSyncItem(id);
  model_->DeleteUninstalledItem(id);
  PruneEmptySyncFolders();
}

void AppListSyncableService::UpdateItem(AppListItem* app_item) {
  // Check to see if the item needs to be moved to/from the OEM folder.
  if (!app_list::switches::IsFolderUIEnabled())
    return;
  bool is_oem = AppIsOem(app_item->id());
  if (!is_oem && app_item->folder_id() == kOemFolderId)
    model_->MoveItemToFolder(app_item, "");
  else if (is_oem && app_item->folder_id() != kOemFolderId)
    model_->MoveItemToFolder(app_item, kOemFolderId);
}

void AppListSyncableService::RemoveSyncItem(const std::string& id) {
  VLOG(2) << this << ": RemoveSyncItem: " << id.substr(0, 8);
  SyncItemMap::iterator iter = sync_items_.find(id);
  if (iter == sync_items_.end()) {
    DVLOG(2) << this << " : RemoveSyncItem: No Item.";
    return;
  }

  // Check for existing RemoveDefault sync item.
  SyncItem* sync_item = iter->second;
  sync_pb::AppListSpecifics::AppListItemType type = sync_item->item_type;
  if (type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    // RemoveDefault item exists, just return.
    DVLOG(2) << this << " : RemoveDefault Item exists.";
    return;
  }

  if (type == sync_pb::AppListSpecifics::TYPE_APP &&
      AppIsDefault(extension_system_->extension_service(), id)) {
    // This is a Default app; update the entry to a REMOVE_DEFAULT entry. This
    // will overwrite any existing entry for the item.
    VLOG(2) << this << " -> SYNC UPDATE: REMOVE_DEFAULT: "
            << sync_item->item_id;
    sync_item->item_type = sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP;
    SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
    return;
  }

  DeleteSyncItem(sync_item);
}

void AppListSyncableService::ResolveFolderPositions() {
  if (!app_list::switches::IsFolderUIEnabled())
    return;

  VLOG(1) << "ResolveFolderPositions.";
  for (SyncItemMap::iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ++iter) {
    SyncItem* sync_item = iter->second;
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;
    AppListItem* app_item = model_->FindItem(sync_item->item_id);
    if (!app_item)
      continue;
    UpdateAppItemFromSyncItem(sync_item, app_item);
  }

  // Move the OEM folder if one exists and we have not synced its position.
  AppListFolderItem* oem_folder = model_->FindFolderItem(kOemFolderId);
  if (oem_folder && !FindSyncItem(kOemFolderId)) {
    model_->SetItemPosition(oem_folder, GetOemFolderPos());
    VLOG(1) << "Creating new OEM folder sync item: "
            << oem_folder->position().ToDebugString();
    CreateSyncItemFromAppItem(oem_folder);
  }
}

void AppListSyncableService::PruneEmptySyncFolders() {
  if (!app_list::switches::IsFolderUIEnabled())
    return;

  std::set<std::string> parent_ids;
  for (SyncItemMap::iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ++iter) {
    parent_ids.insert(iter->second->parent_id);
  }
  for (SyncItemMap::iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ) {
    SyncItem* sync_item = (iter++)->second;
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;
    if (!ContainsKey(parent_ids, sync_item->item_id))
      DeleteSyncItem(sync_item);
  }
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

  // Ensure the model is built.
  GetModel();

  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(error_handler);
  if (switches::IsFolderUIEnabled())
    model_->SetFoldersEnabled(true);

  syncer::SyncMergeResult result = syncer::SyncMergeResult(type);
  result.set_num_items_before_association(sync_items_.size());
  VLOG(1) << this << ": MergeDataAndStartSyncing: "
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
    const std::string& item_id = data.GetSpecifics().app_list().item_id();
    const sync_pb::AppListSpecifics& specifics = data.GetSpecifics().app_list();
    DVLOG(2) << this << "  Initial Sync Item: " << item_id
             << " Type: " << specifics.item_type();
    DCHECK_EQ(syncer::APP_LIST, data.GetDataType());
    if (ProcessSyncItemSpecifics(specifics))
      ++new_items;
    else
      ++updated_items;
    if (specifics.item_type() != sync_pb::AppListSpecifics::TYPE_FOLDER &&
        !IsUnRemovableDefaultApp(item_id) &&
        !AppIsOem(item_id) &&
        !AppIsDefault(extension_system_->extension_service(), item_id)) {
      VLOG(2) << "Syncing non-default item: " << item_id;
      first_app_list_sync_ = false;
    }
    unsynced_items.erase(item_id);
  }
  result.set_num_items_after_association(sync_items_.size());
  result.set_num_items_added(new_items);
  result.set_num_items_deleted(0);
  result.set_num_items_modified(updated_items);

  // Initial sync data has been processed, it is safe now to add new sync items.
  initial_sync_data_processed_ = true;

  // Send unsynced items. Does not affect |result|.
  syncer::SyncChangeList change_list;
  for (std::set<std::string>::iterator iter = unsynced_items.begin();
       iter != unsynced_items.end(); ++iter) {
    SyncItem* sync_item = FindSyncItem(*iter);
    // Sync can cause an item to change folders, causing an unsynced folder
    // item to be removed.
    if (!sync_item)
      continue;
    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
    change_list.push_back(SyncChange(FROM_HERE,  SyncChange::ACTION_ADD,
                                     GetSyncDataFromSyncItem(sync_item)));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);

  // Adding items may have created folders without setting their positions
  // since we haven't started observing the item list yet. Resolve those.
  ResolveFolderPositions();

  // Start observing app list model changes.
  model_observer_.reset(new ModelObserver(this));

  return result;
}

void AppListSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::APP_LIST);

  sync_processor_.reset();
  sync_error_handler_.reset();
  model_->SetFoldersEnabled(false);
}

syncer::SyncDataList AppListSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::APP_LIST, type);

  VLOG(1) << this << ": GetAllSyncData: " << sync_items_.size();
  syncer::SyncDataList list;
  for (SyncItemMap::const_iterator iter = sync_items_.begin();
       iter != sync_items_.end(); ++iter) {
    VLOG(2) << this << " -> SYNC: " << iter->second->ToString();
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

  // Don't observe the model while processing incoming sync changes.
  model_observer_.reset();

  VLOG(1) << this << ": ProcessSyncChanges: " << change_list.size();
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    const SyncChange& change = *iter;
    VLOG(2) << this << "  Change: "
            << change.sync_data().GetSpecifics().app_list().item_id()
            << " (" << change.change_type() << ")";
    if (change.change_type() == SyncChange::ACTION_ADD ||
        change.change_type() == SyncChange::ACTION_UPDATE) {
      ProcessSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else if (change.change_type() == SyncChange::ACTION_DELETE) {
      DeleteSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else {
      LOG(ERROR) << "Invalid sync change";
    }
  }

  // Continue observing app list model changes.
  model_observer_.reset(new ModelObserver(this));

  return syncer::SyncError();
}

// AppListSyncableService private

bool AppListSyncableService::ProcessSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "AppList item with empty ID";
    return false;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If an item of the same type exists, update it.
    if (sync_item->item_type == specifics.item_type()) {
      UpdateSyncItemFromSync(specifics, sync_item);
      ProcessExistingSyncItem(sync_item);
      VLOG(2) << this << " <- SYNC UPDATE: " << sync_item->ToString();
      return false;
    }
    // Otherwise, one of the entries should be TYPE_REMOVE_DEFAULT_APP.
    if (sync_item->item_type !=
        sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP &&
        specifics.item_type() !=
        sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      LOG(ERROR) << "Synced item type: " << specifics.item_type()
                 << " != existing sync item type: " << sync_item->item_type
                 << " Deleting item from model!";
      model_->DeleteItem(item_id);
    }
    VLOG(2) << this << " - ProcessSyncItem: Delete existing entry: "
            << sync_item->ToString();
    delete sync_item;
    sync_items_.erase(item_id);
  }

  sync_item = CreateSyncItem(item_id, specifics.item_type());
  UpdateSyncItemFromSync(specifics, sync_item);
  ProcessNewSyncItem(sync_item);
  VLOG(2) << this << " <- SYNC ADD: " << sync_item->ToString();
  return true;
}

void AppListSyncableService::ProcessNewSyncItem(SyncItem* sync_item) {
  VLOG(2) << "ProcessNewSyncItem: " << sync_item->ToString();
  switch (sync_item->item_type) {
    case sync_pb::AppListSpecifics::TYPE_APP: {
      // New apps are added through ExtensionAppModelBuilder.
      // TODO(stevenjb): Determine how to handle app items in sync that
      // are not installed (e.g. default / OEM apps).
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP: {
      VLOG(1) << this << ": Uninstall: " << sync_item->ToString();
      if (IsDriveAppSyncId(sync_item->item_id)) {
        if (drive_app_provider_) {
          drive_app_provider_->AddUninstalledDriveAppFromSync(
              GetDriveAppIdFromSyncId(sync_item->item_id));
        }
      } else {
        UninstallExtension(extension_system_->extension_service(),
                           sync_item->item_id);
      }
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_FOLDER: {
      AppListItem* app_item = model_->FindItem(sync_item->item_id);
      if (!app_item)
        return;  // Don't create new folders here, the model will do that.
      UpdateAppItemFromSyncItem(sync_item, app_item);
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_URL: {
      // TODO(stevenjb): Implement
      LOG(WARNING) << "TYPE_URL not supported";
      return;
    }
  }
  NOTREACHED() << "Unrecognized sync item type: " << sync_item->ToString();
}

void AppListSyncableService::ProcessExistingSyncItem(SyncItem* sync_item) {
  if (sync_item->item_type ==
      sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    return;
  }
  VLOG(2) << "ProcessExistingSyncItem: " << sync_item->ToString();
  AppListItem* app_item = model_->FindItem(sync_item->item_id);
  DVLOG(2) << " AppItem: " << app_item->ToDebugString();
  if (!app_item) {
    LOG(ERROR) << "Item not found in model: " << sync_item->ToString();
    return;
  }
  // This is the only place where sync can cause an item to change folders.
  if (app_list::switches::IsFolderUIEnabled() &&
      app_item->folder_id() != sync_item->parent_id &&
      !AppIsOem(app_item->id())) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    model_->MoveItemToFolder(app_item, sync_item->parent_id);
  }
  UpdateAppItemFromSyncItem(sync_item, app_item);
}

void AppListSyncableService::UpdateAppItemFromSyncItem(
    const AppListSyncableService::SyncItem* sync_item,
    AppListItem* app_item) {
  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  if (!app_item->position().Equals(sync_item->item_ordinal))
    model_->SetItemPosition(app_item, sync_item->item_ordinal);
  // Only update the item name if it is a Folder or the name is empty.
  if (sync_item->item_name != app_item->name() &&
      sync_item->item_id != kOemFolderId &&
      (app_item->GetItemType() == AppListFolderItem::kItemType ||
       app_item->name().empty())) {
    model_->SetItemName(app_item, sync_item->item_name);
  }
}

bool AppListSyncableService::SyncStarted() {
  if (sync_processor_.get())
    return true;
  if (flare_.is_null()) {
    VLOG(1) << this << ": SyncStarted: Flare.";
    flare_ = sync_start_util::GetFlareForSyncableService(profile_->GetPath());
    flare_.Run(syncer::APP_LIST);
  }
  return false;
}

void AppListSyncableService::SendSyncChange(
    SyncItem* sync_item,
    SyncChange::SyncChangeType sync_change_type) {
  if (!SyncStarted()) {
    DVLOG(2) << this << " - SendSyncChange: SYNC NOT STARTED: "
             << sync_item->ToString();
    return;
  }
  if (!initial_sync_data_processed_ &&
      sync_change_type == SyncChange::ACTION_ADD) {
    // This can occur if an initial item is created before its folder item.
    // A sync item should already exist for the folder, so we do not want to
    // send an ADD event, since that would trigger a CHECK in the sync code.
    DCHECK(sync_item->item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
    DVLOG(2) << this << " - SendSyncChange: ADD before initial data processed: "
             << sync_item->ToString();
    return;
  }
  if (sync_change_type == SyncChange::ACTION_ADD)
    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
  else
    VLOG(2) << this << " -> SYNC UPDATE: " << sync_item->ToString();
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

AppListSyncableService::SyncItem*
AppListSyncableService::CreateSyncItem(
    const std::string& item_id,
    sync_pb::AppListSpecifics::AppListItemType item_type) {
  DCHECK(!ContainsKey(sync_items_, item_id));
  SyncItem* sync_item = new SyncItem(item_id, item_type);
  sync_items_[item_id] = sync_item;
  return sync_item;
}

void AppListSyncableService::DeleteSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "Delete AppList item with empty ID";
    return;
  }
  VLOG(2) << this << ": DeleteSyncItemSpecifics: " << item_id.substr(0, 8);
  SyncItemMap::iterator iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return;
  sync_pb::AppListSpecifics::AppListItemType item_type =
      iter->second->item_type;
  VLOG(2) << this << " <- SYNC DELETE: " << iter->second->ToString();
  delete iter->second;
  sync_items_.erase(iter);
  // Only delete apps from the model. Folders will be deleted when all
  // children have been deleted.
  if (item_type == sync_pb::AppListSpecifics::TYPE_APP) {
    model_->DeleteItem(item_id);
  } else if (item_type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    if (IsDriveAppSyncId(item_id) && drive_app_provider_) {
      drive_app_provider_->RemoveUninstalledDriveAppFromSync(
          GetDriveAppIdFromSyncId(item_id));
    }
  }
}

std::string AppListSyncableService::FindOrCreateOemFolder() {
  AppListFolderItem* oem_folder = model_->FindFolderItem(kOemFolderId);
  if (!oem_folder) {
    scoped_ptr<AppListFolderItem> new_folder(new AppListFolderItem(
        kOemFolderId, AppListFolderItem::FOLDER_TYPE_OEM));
    oem_folder =
        static_cast<AppListFolderItem*>(model_->AddItem(std::move(new_folder)));
    SyncItem* oem_sync_item = FindSyncItem(kOemFolderId);
    if (oem_sync_item) {
      VLOG(1) << "Creating OEM folder from existing sync item: "
               << oem_sync_item->item_ordinal.ToDebugString();
      model_->SetItemPosition(oem_folder, oem_sync_item->item_ordinal);
    } else {
      model_->SetItemPosition(oem_folder, GetOemFolderPos());
      // Do not create a sync item for the OEM folder here, do it in
      // ResolveFolderPositions() when the item position is finalized.
    }
  }
  model_->SetItemName(oem_folder, oem_folder_name_);
  return oem_folder->id();
}

syncer::StringOrdinal AppListSyncableService::GetOemFolderPos() {
  VLOG(1) << "GetOemFolderPos: " << first_app_list_sync_;
  if (!first_app_list_sync_) {
    VLOG(1) << "Sync items exist, placing OEM folder at end.";
    syncer::StringOrdinal last;
    for (SyncItemMap::iterator iter = sync_items_.begin();
         iter != sync_items_.end(); ++iter) {
      SyncItem* sync_item = iter->second;
      if (sync_item->item_ordinal.IsValid() &&
          (!last.IsValid() || sync_item->item_ordinal.GreaterThan(last))) {
        last = sync_item->item_ordinal;
      }
    }
    if (last.IsValid())
      return last.CreateAfter();
  }

  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  AppListItemList* item_list = model_->top_level_item_list();
  if (item_list->item_count() == 0)
    return syncer::StringOrdinal();

  size_t oem_index = 0;
  for (; oem_index < item_list->item_count() - 1; ++oem_index) {
    AppListItem* cur_item = item_list->item_at(oem_index);
    if (cur_item->id() == extensions::kWebStoreAppId)
      break;
  }
  syncer::StringOrdinal oem_ordinal;
  AppListItem* prev = item_list->item_at(oem_index);
  if (oem_index + 1 < item_list->item_count()) {
    AppListItem* next = item_list->item_at(oem_index + 1);
    oem_ordinal = prev->position().CreateBetween(next->position());
  } else {
    oem_ordinal = prev->position().CreateAfter();
  }
  VLOG(1) << "Placing OEM Folder at: " << oem_index
          << " position: " << oem_ordinal.ToDebugString();
  return oem_ordinal;
}

bool AppListSyncableService::AppIsOem(const std::string& id) {
  if (!extension_system_->extension_service())
    return false;
  const extensions::Extension* extension =
      extension_system_->extension_service()->GetExtensionById(id, true);
  return extension && extension->was_installed_by_oem();
}

std::string AppListSyncableService::SyncItem::ToString() const {
  std::string res = item_id.substr(0, 8);
  if (item_type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    res += " { RemoveDefault }";
  } else {
    res += " { " + item_name + " }";
    res += " [" + item_ordinal.ToDebugString() + "]";
    if (!parent_id.empty())
      res += " <" + parent_id.substr(0, 8) + ">";
  }
  return res;
}

}  // namespace app_list
