// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

class AppListControllerDelegate;
class ChromeAppListItem;
class Profile;

// This abstract class populates and maintains the given |model| with
// information from |profile| for the specific item type.
class AppListModelBuilder {
 public:
  // |controller| is owned by implementation of AppListService.
  AppListModelBuilder(AppListControllerDelegate* controller,
                      const char* item_type);
  virtual ~AppListModelBuilder();

  // Initialize to use app-list sync and sets |service_| to |service|.
  // |service| is the owner of this instance.
  void Initialize(app_list::AppListSyncableService* service,
                  Profile* profile,
                  AppListModelUpdater* model_updater);

 protected:
  // Builds the model with the current profile.
  virtual void BuildModel() = 0;

  app_list::AppListSyncableService* service() { return service_; }

  Profile* profile() { return profile_; }

  AppListControllerDelegate* controller() { return controller_; }

  AppListModelUpdater* model_updater() { return model_updater_; }

  // Inserts an app based on app ordinal prefs.
  virtual void InsertApp(std::unique_ptr<ChromeAppListItem> app);

  // Removes an app based on app id. If |unsynced_change| is set to true then
  // app is removed only from model and sync service is not used.
  virtual void RemoveApp(const std::string& id, bool unsynced_change);

  // Returns a SyncItem of the specified type if it exists.
  const app_list::AppListSyncableService::SyncItem* GetSyncItem(
      const std::string& id,
      sync_pb::AppListSpecifics::AppListItemType type);

  // Returns app instance matching |id| or nullptr.
  ChromeAppListItem* GetAppItem(const std::string& id);

 private:
  // Unowned pointers to the service that owns this and associated profile.
  app_list::AppListSyncableService* service_ = nullptr;
  Profile* profile_ = nullptr;

  // Unowned pointer to an app list model updater.
  AppListModelUpdater* model_updater_ = nullptr;

  // Unowned pointer to the app list controller.
  AppListControllerDelegate* controller_;

  // Global constant defined for each item type.
  const char* item_type_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_
