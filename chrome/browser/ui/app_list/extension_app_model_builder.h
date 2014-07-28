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
  virtual ~ExtensionAppModelBuilder();

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
  virtual void OnBeginExtensionInstall(
      const ExtensionInstallParams& params) OVERRIDE;
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;
  virtual void OnInstallFailure(const std::string& extension_id) OVERRIDE;
  virtual void OnDisabledExtensionUpdated(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnAppInstalledToAppList(
      const std::string& extension_id) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // extensions::ExtensionRegistryObserver.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;
  virtual void OnShutdown(extensions::ExtensionRegistry* registry) OVERRIDE;

  // AppListItemListObserver.
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               app_list::AppListItem* item) OVERRIDE;

  scoped_ptr<ExtensionAppItem> CreateAppItem(
      const std::string& extension_id,
      const std::string& extension_name,
      const gfx::ImageSkia& installing_icon,
      bool is_platform_app);

  // Populates the model with apps.
  void PopulateApps();

  // Inserts an app based on app ordinal prefs.
  void InsertApp(scoped_ptr<ExtensionAppItem> app);

  // Sets which app is intended to be highlighted. Will remove the highlight
  // from a currently highlighted app.
  void SetHighlightedApp(const std::string& extension_id);

  // Sets the application app with |highlight_app_id_| in |model_| as
  // highlighted if |highlighted_app_pending_| is true. If such an app is found,
  // reset |highlighted_app_pending_| so that won't be highlighted again until
  // another call to SetHighlightedApp() is made.
  void UpdateHighlight();

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

  std::string highlight_app_id_;

  // True if we haven't set |highlight_app_id_| to be highlighted. This happens
  // if we try to highlight an app that doesn't exist in the list yet.
  bool highlighted_app_pending_;

  // We listen to this to show app installing progress.
  extensions::InstallTracker* tracker_;

  // Listen extension's load, unload, uninstalled.
  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_
