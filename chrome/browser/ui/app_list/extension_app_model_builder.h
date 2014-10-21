// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/install_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/app_list/app_list_model.h"
#include "ui/base/models/list_model_observer.h"

class AppListControllerDelegate;
class ExtensionAppItem;
class Profile;

namespace app_list {
class AppListSyncableService;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
class ExtensionSet;
class InstallTracker;
}

namespace gfx {
class ImageSkia;
}

// This class populates and maintains the given |model| with information from
// |profile|.
class ExtensionAppModelBuilder : public extensions::InstallObserver,
                                 public extensions::ExtensionRegistryObserver,
                                 public app_list::AppListItemListObserver {
 public:
  explicit ExtensionAppModelBuilder(AppListControllerDelegate* controller);
  ~ExtensionAppModelBuilder() override;

  // Initialize to use app-list sync and sets |service_| to |service|.
  void InitializeWithService(app_list::AppListSyncableService* service);

  // Initialize to use extension sync and sets |service_| to NULL. Used in
  // tests and when AppList sync is not enabled.
  void InitializeWithProfile(Profile* profile, app_list::AppListModel* model);

 private:
  typedef std::vector<ExtensionAppItem*> ExtensionAppList;

  // Builds the model with the current profile.
  void BuildModel();

  // extensions::InstallObserver.
  void OnBeginExtensionInstall(const ExtensionInstallParams& params) override;
  void OnDownloadProgress(const std::string& extension_id,
                          int percent_downloaded) override;
  void OnInstallFailure(const std::string& extension_id) override;
  void OnDisabledExtensionUpdated(
      const extensions::Extension* extension) override;
  void OnShutdown() override;

  // extensions::ExtensionRegistryObserver.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // AppListItemListObserver.
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       app_list::AppListItem* item) override;

  scoped_ptr<ExtensionAppItem> CreateAppItem(
      const std::string& extension_id,
      const std::string& extension_name,
      const gfx::ImageSkia& installing_icon,
      bool is_platform_app);

  // Populates the model with apps.
  void PopulateApps();

  // Inserts an app based on app ordinal prefs.
  void InsertApp(scoped_ptr<ExtensionAppItem> app);

  // Returns app instance matching |extension_id| or NULL.
  ExtensionAppItem* GetExtensionAppItem(const std::string& extension_id);

  // Initializes the |profile_pref_change_registrar_| and the
  // |extension_pref_change_registrar_| to listen for changes to profile and
  // extension prefs, and call OnProfilePreferenceChanged() or
  // OnExtensionPreferenceChanged().
  void InitializePrefChangeRegistrars();

  // Handles profile prefs changes.
  void OnProfilePreferenceChanged();

  // Handles extension prefs changes.
  void OnExtensionPreferenceChanged();

  // Unowned pointers to the service that owns this and associated profile.
  app_list::AppListSyncableService* service_;
  Profile* profile_;

  // Registrar used to monitor the profile prefs.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // Registrar used to monitor the extension prefs.
  PrefChangeRegistrar extension_pref_change_registrar_;

  // Unowned pointer to the app list controller.
  AppListControllerDelegate* controller_;

  // Unowned pointer to the app list model.
  app_list::AppListModel* model_;

  // We listen to this to show app installing progress.
  extensions::InstallTracker* tracker_;

  // Listen extension's load, unload, uninstalled.
  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_
