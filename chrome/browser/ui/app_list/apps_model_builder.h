// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/extensions/install_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_model.h"
#include "ui/base/models/list_model_observer.h"

class AppListControllerDelegate;
class ExtensionAppItem;
class ExtensionSet;
class Profile;

namespace gfx {
class ImageSkia;
}

class AppsModelBuilder : public content::NotificationObserver,
                         public ui::ListModelObserver,
                         public extensions::InstallObserver {
 public:
  AppsModelBuilder(Profile* profile,
                   app_list::AppListModel::Apps* model,
                   AppListControllerDelegate* controller);
  virtual ~AppsModelBuilder();

  // Populates the model.
  void Build();

 private:
  typedef std::vector<ExtensionAppItem*> Apps;

  // Overridden from extensions::InstallObserver:
  virtual void OnBeginExtensionInstall(const std::string& extension_id,
                                       const std::string& extension_name,
                                       const gfx::ImageSkia& installing_icon,
                                       bool is_app) OVERRIDE;

  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;

  virtual void OnInstallFailure(const std::string& extension_id) OVERRIDE;

  // Adds apps in |extensions| to |apps|.
  void AddApps(const ExtensionSet* extensions, Apps* apps);

  // Populates the model with apps.
  void PopulateApps();

  // Re-sort apps in case app ordinal prefs are changed.
  void ResortApps();

  // Inserts an app based on app ordinal prefs.
  void InsertApp(ExtensionAppItem* app);

  // Returns the index of the application app with |app_id| in |model_|. If
  // no match is found, returns -1.
  int FindApp(const std::string& app_id);

  // Sets the application app with |highlight_app_id_| in |model_| as
  // highlighted. If such an app is found, reset |highlight_app_id_| so that it
  // is highlighted once per install notification.
  void HighlightApp();

  // Returns app instance at given |index|.
  ExtensionAppItem* GetAppAt(size_t index);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ui::ListModelObserver overrides:
  virtual void ListItemsAdded(size_t start, size_t count) OVERRIDE;
  virtual void ListItemsRemoved(size_t start, size_t count) OVERRIDE;
  virtual void ListItemMoved(size_t index, size_t target_index) OVERRIDE;
  virtual void ListItemsChanged(size_t start, size_t count) OVERRIDE;

  Profile* profile_;
  AppListControllerDelegate* controller_;

  // Sub apps model of AppListModel that represents apps grid view.
  app_list::AppListModel::Apps* model_;

  std::string highlight_app_id_;

  // True to ignore |model_| changes.
  bool ignore_changes_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppsModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_
