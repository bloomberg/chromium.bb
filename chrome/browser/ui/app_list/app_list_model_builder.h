// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/app_list_model.h"

class AppListControllerDelegate;
class Profile;

// This abstract class populates and maintains the given |model| with
// information from |profile| for the specific item type.
class AppListModelBuilder : public app_list::AppListItemListObserver {
 public:
  // |controller| is owned by implementation of AppListService.
  AppListModelBuilder(AppListControllerDelegate* controller,
                      const char* item_type);
   ~AppListModelBuilder() override;

  // Initialize to use app-list sync and sets |service_| to |service|.
  // |service| is the owner of this instance and |model|.
  void InitializeWithService(app_list::AppListSyncableService* service,
                             app_list::AppListModel* model);

  // Initialize to use extension sync and sets |service_| to nullptr. Used in
  // tests and when AppList sync is not enabled.
  // app_list::AppListSyncableService is the owner of |model|
  void InitializeWithProfile(Profile* profile, app_list::AppListModel* model);

 protected:
  // Builds the model with the current profile.
  virtual void BuildModel() = 0;

  app_list::AppListSyncableService* service() { return service_; }

  Profile* profile() { return profile_; }

  AppListControllerDelegate* controller() { return controller_; }

  app_list::AppListModel* model() { return model_; }

  // Inserts an app based on app ordinal prefs.
  void InsertApp(std::unique_ptr<app_list::AppListItem> app);

  // Removes an app based on app id. If |unsynced_change| is set to true then
  // app is removed only from model and sync service is not used.
  void RemoveApp(const std::string& id, bool unsynced_change);

  const app_list::AppListSyncableService::SyncItem* GetSyncItem(
      const std::string& id);

  // Returns app instance matching |id| or nullptr.
  app_list::AppListItem* GetAppItem(const std::string& id);

 private:
  // Unowned pointers to the service that owns this and associated profile.
  app_list::AppListSyncableService* service_ = nullptr;
  Profile* profile_ = nullptr;

  // Unowned pointer to the app list model.
  app_list::AppListModel* model_ = nullptr;

  // Unowned pointer to the app list controller.
  AppListControllerDelegate* controller_;

  // Global constant defined for each item type.
  const char* item_type_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_BUILDER_H_
