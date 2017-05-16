// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/drive/drive_app_uninstall_sync_service.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/app_list_specifics.pb.h"

class ArcAppModelBuilder;
class DriveAppProvider;
class ExtensionAppModelBuilder;
class Profile;

namespace extensions {
class ExtensionSystem;
}

namespace sync_pb {
class AppListSpecifics;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace app_list {

class AppListItem;
class AppListModel;

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
class AppListSyncableService : public syncer::SyncableService,
                               public KeyedService,
                               public DriveAppUninstallSyncService {
 public:
  struct SyncItem {
    SyncItem(const std::string& id,
             sync_pb::AppListSpecifics::AppListItemType type);
    ~SyncItem();
    const std::string item_id;
    sync_pb::AppListSpecifics::AppListItemType item_type;
    std::string item_name;
    std::string parent_id;
    syncer::StringOrdinal item_ordinal;
    syncer::StringOrdinal item_pin_ordinal;

    std::string ToString() const;
  };

  class Observer {
   public:
    // Notifies that sync model was updated.
    virtual void OnSyncModelUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  using SyncItemMap = std::map<std::string, std::unique_ptr<SyncItem>>;

  // Populates the model when |extension_system| is ready.
  AppListSyncableService(Profile* profile,
                         extensions::ExtensionSystem* extension_system);

  ~AppListSyncableService() override;

  static const char kOemFolderId[];

  // Registers prefs to support local storage.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Adds |item| to |sync_items_| and |model_|. If a sync item already exists,
  // updates the existing sync item instead.
  void AddItem(std::unique_ptr<AppListItem> app_item);

  // Removes sync item matching |id|.
  void RemoveItem(const std::string& id);

  // Removes sync item matching |id| after item uninstall.
  void RemoveUninstalledItem(const std::string& id);

  // Called when properties of an item may have changed, e.g. default/oem state.
  void UpdateItem(AppListItem* app_item);

  // Returns the existing sync item matching |id| or NULL.
  const SyncItem* GetSyncItem(const std::string& id) const;

  // Sets the name of the folder for OEM apps.
  void SetOemFolderName(const std::string& name);

  // Returns optional pin position for the app specified by |app_id|. If app is
  // not synced or does not have associated pin position then empty ordinal is
  // returned.
  syncer::StringOrdinal GetPinPosition(const std::string& app_id);

  // Sets pin position and how it is pinned for the app specified by |app_id|.
  // Empty |item_pin_ordinal| indicates that the app has no pin.
  void SetPinPosition(const std::string& app_id,
                      const syncer::StringOrdinal& item_pin_ordinal);

  // Gets the app list model.
  AppListModel* GetModel();

  // Returns true if this service was initialized.
  bool IsInitialized() const;

  // Registers new observers and makes sure that service is started.
  void AddObserverAndStart(Observer* observer);
  void RemoveObserver(Observer* observer);

  Profile* profile() { return profile_; }
  size_t GetNumSyncItemsForTest();
  const std::string& GetOemFolderNameForTest() const {
    return oem_folder_name_;
  }
  void ResetDriveAppProviderForTest();

  const SyncItemMap& sync_items() const { return sync_items_; }

  // syncer::SyncableService
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  class ModelObserver;

  // KeyedService
  void Shutdown() override;

  // DriveAppUninstallSyncService
  void TrackUninstalledDriveApp(const std::string& drive_app_id) override;
  void UntrackUninstalledDriveApp(const std::string& drive_app_id) override;

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
  void DeleteSyncItem(const std::string& item_id);

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

  // Initializes sync items from the local storage while sync service is not
  // enabled.
  void InitFromLocalStorage();

  // Helper that notifies observers that sync model has been updated.
  void NotifyObserversSyncUpdated();

  // Handles model update start/finish.
  void HandleUpdateStarted();
  void HandleUpdateFinished();

  // Returns true if extension service is ready.
  bool IsExtensionServiceReady() const;

  // Play Store app id is changed in the app launcher and now unified with shelf
  // id. This copies position from the legacy Play Store item in case the legacy
  // position was modified and differs from the default position and the new
  // position is still default. Don't remove the legacy sync item once user may
  // use old and new versions at the same time.
  // TODO(khmel): Remove import of legacy Play Store sync item after few
  // releases http://crbug.com/722675.
  void MaybeImportLegacyPlayStorePosition(syncer::SyncChangeList* change_list);

  Profile* profile_;
  extensions::ExtensionSystem* extension_system_;
  std::unique_ptr<AppListModel> model_;
  std::unique_ptr<ModelObserver> model_observer_;
  std::unique_ptr<ExtensionAppModelBuilder> apps_builder_;
  std::unique_ptr<ArcAppModelBuilder> arc_apps_builder_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_handler_;
  SyncItemMap sync_items_;
  syncer::SyncableService::StartSyncFlare flare_;
  bool initial_sync_data_processed_;
  bool first_app_list_sync_;
  std::string oem_folder_name_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  // Provides integration with Drive apps.
  std::unique_ptr<DriveAppProvider> drive_app_provider_;

  base::WeakPtrFactory<AppListSyncableService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
