// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"
#include "sync/protocol/app_list_specifics.pb.h"

class ExtensionAppModelBuilder;
class ExtensionService;
class Profile;

namespace sync_pb {
class AppListSpecifics;
}

namespace app_list {

class AppListModel;
class AppListItemModel;

// Keyed Service that owns, stores, and syncs an AppListModel for an
// ExtensionSystem and corresponding profile.
class AppListSyncableService : public syncer::SyncableService,
                               public BrowserContextKeyedService,
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
  };

  // Create an empty model. Then, if |extension_service| is non-NULL and ready,
  // populate it. Otherwise populate the model once extensions become ready.
  AppListSyncableService(Profile* profile, ExtensionService* extension_service);

  virtual ~AppListSyncableService();

  // TODO(stevenjb): Implement specific Add and Update methods for
  // ExtensionAppItem, etc.

  // Removes sync item matching |id|.
  void RemoveItem(const std::string& id);

  AppListModel* model() { return model_.get(); }

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
  typedef std::map<std::string, SyncItem*> SyncItemMap;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Builds the model once ExtensionService is ready.
  void BuildModel();

  // Returns true if sync has started, otherwise creates and runs |flare_|.
  bool SyncStarted();

  // Common functionality for adding items.
  SyncItem* AddItem(sync_pb::AppListSpecifics::AppListItemType type,
                    AppListItemModel* item);

  // Common functionality for updating items.
  void UpdateItem(AppListItemModel* item);

  // Returns an existing SyncItem corresponding to |item_id| or NULL.
  SyncItem* FindSyncItem(const std::string& item_id);

  // Returns a SyncItem corresponding to |item_id|. Sets |new_item| if an item
  // was created.
  SyncItem* FindOrCreateSyncItem(
      const std::string& item_id,
      sync_pb::AppListSpecifics::AppListItemType type,
      bool* new_item);

  // Creates or updates a SyncItem from |specifics|. Returns true if an item
  // was created.
  bool CreateOrUpdateSyncItem(const sync_pb::AppListSpecifics& specifics);

  // Deletes a SyncItem matching |specifics|.
  void DeleteSyncItem(const sync_pb::AppListSpecifics& specifics);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<AppListModel> model_;
  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;
  SyncItemMap sync_items_;
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
