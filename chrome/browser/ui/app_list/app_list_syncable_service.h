// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"
#include "sync/protocol/app_list_specifics.pb.h"

class DriveAppProvider;
class ExtensionAppModelBuilder;
class Profile;

namespace extensions {
class ExtensionSystem;
}

namespace sync_pb {
class AppListSpecifics;
}

namespace app_list {

class AppListFolderItem;
class AppListItem;
class AppListModel;

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
class AppListSyncableService : public syncer::SyncableService,
                               public KeyedService,
                               public content::NotificationObserver {
 public:
  struct SyncItem {
    SyncItem(const std::string& id,
             sync_pb::AppListSpecifics::AppListItemType type);
    ~SyncItem();
    const std::string item_id;
    sync_pb::AppListSpecifics::AppListItemType item_type;
    std::string item_name;
    std::string parent_id;
    syncer::StringOrdinal page_ordinal;
    syncer::StringOrdinal item_ordinal;

    std::string ToString() const;
  };

  // Populates the model when |extension_system| is ready.
  AppListSyncableService(Profile* profile,
                         extensions::ExtensionSystem* extension_system);

  virtual ~AppListSyncableService();

  // Adds |item| to |sync_items_| and |model_|. If a sync item already exists,
  // updates the existing sync item instead.
  void AddItem(scoped_ptr<AppListItem> app_item);

  // Removes sync item matching |id|.
  void RemoveItem(const std::string& id);

  // Called when properties of an item may have changed, e.g. default/oem state.
  void UpdateItem(AppListItem* app_item);

  // Returns the existing sync item matching |id| or NULL.
  const SyncItem* GetSyncItem(const std::string& id) const;

  // Sets the name of the folder for OEM apps.
  void SetOemFolderName(const std::string& name);

  Profile* profile() { return profile_; }
  AppListModel* model() { return model_.get(); }
  size_t GetNumSyncItemsForTest() const { return sync_items_.size(); }
  const std::string& GetOemFolderNameForTest() const {
    return oem_folder_name_;
  }
  void ResetDriveAppProviderForTest();

  // syncer::SyncableService
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  class ModelObserver;
  typedef std::map<std::string, SyncItem*> SyncItemMap;

  // KeyedService
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Builds the model once ExtensionService is ready.
  void BuildModel();

  // Returns true if sync has restarted, otherwise runs |flare_|.
  bool SyncStarted();

  // If |app_item| matches an existing sync item, returns it. Otherwise adds
  // |app_item| to |sync_items_| and returns the new item. If |app_item| is
  // invalid returns NULL.
  SyncItem* FindOrAddSyncItem(AppListItem* app_item);

  // Creates a sync item for |app_item| and sends an ADD SyncChange event.
  SyncItem* CreateSyncItemFromAppItem(AppListItem* app_item);

  // If a sync item for |app_item| already exists, update |app_item| from the
  // sync item, otherwise create a new sync item from |app_item|.
  void AddOrUpdateFromSyncItem(AppListItem* app_item);

  // Either uninstalling a default app or remove the REMOVE_DEFAULT sync item.
  // Returns true if the app is removed. Otherwise deletes the existing sync
  // item and returns false.
  bool RemoveDefaultApp(AppListItem* item, SyncItem* sync_item);

  // Deletes a sync item from |sync_items_| and sends a DELETE action.
  void DeleteSyncItem(SyncItem* sync_item);

  // Updates existing entry in |sync_items_| from |app_item|.
  void UpdateSyncItem(AppListItem* app_item);

  // Removes sync item matching |id|.
  void RemoveSyncItem(const std::string& id);

  // Updates folder items that may get created during initial sync.
  void ResolveFolderPositions();

  // Removes any empty SyncItem folders and deletes them from sync. Called
  // after a sync item is removed (which may result in an empty folder).
  void PruneEmptySyncFolders();

  // Creates or updates a SyncItem from |specifics|. Returns true if a new item
  // was created.
  bool ProcessSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Handles a newly created sync item (e.g. creates a new AppItem and adds it
  // to the model or uninstalls a deleted default item.
  void ProcessNewSyncItem(SyncItem* sync_item);

  // Handles an existing sync item.
  void ProcessExistingSyncItem(SyncItem* sync_item);

  // Updates |app_item| from |sync_item| (e.g. updates item positions).
  void UpdateAppItemFromSyncItem(const SyncItem* sync_item,
                                 AppListItem* app_item);

  // Sends ADD or CHANGED for sync item.
  void SendSyncChange(SyncItem* sync_item,
                      syncer::SyncChange::SyncChangeType sync_change_type);

  // Returns an existing SyncItem corresponding to |item_id| or NULL.
  SyncItem* FindSyncItem(const std::string& item_id);

  // Creates a new sync item for |item_id|.
  SyncItem* CreateSyncItem(
      const std::string& item_id,
      sync_pb::AppListSpecifics::AppListItemType item_type);

  // Deletes a SyncItem matching |specifics|.
  void DeleteSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Creates the OEM folder and sets its name if necessary. Returns the OEM
  // folder id.
  std::string FindOrCreateOemFolder();

  // Gets the location for the OEM folder. Called when the folder is first
  // created.
  syncer::StringOrdinal GetOemFolderPos();

  // Returns true if an extension matching |id| exists and was installed by
  // an OEM (extension->was_installed_by_oem() is true).
  bool AppIsOem(const std::string& id);

  Profile* profile_;
  extensions::ExtensionSystem* extension_system_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<AppListModel> model_;
  scoped_ptr<ModelObserver> model_observer_;
  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;
  SyncItemMap sync_items_;
  syncer::SyncableService::StartSyncFlare flare_;
  bool initial_sync_data_processed_;
  bool first_app_list_sync_;
  std::string oem_folder_name_;

  // Provides integration with Drive apps.
  scoped_ptr<DriveAppProvider> drive_app_provider_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
