// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/ui/app_list/app_list_model_builder.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/models/list_model_observer.h"

class AppListControllerDelegate;
class ExtensionAppItem;

namespace extensions {
class Extension;
class ExtensionRegistry;
class ExtensionSet;
class InstallTracker;
}

namespace gfx {
class ImageSkia;
}

// This class populates and maintains the given |model| for extension items
// with information from |profile|.
class ExtensionAppModelBuilder : public AppListModelBuilder,
                                 public extensions::InstallObserver,
                                 public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionAppModelBuilder(AppListControllerDelegate* controller);
  ~ExtensionAppModelBuilder() override;

 private:
  typedef std::vector<ExtensionAppItem*> ExtensionAppList;

  // AppListModelBuilder
  void BuildModel() override;

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

  // Registrar used to monitor the profile prefs.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // Registrar used to monitor the extension prefs.
  PrefChangeRegistrar extension_pref_change_registrar_;

  // We listen to this to show app installing progress.
  extensions::InstallTracker* tracker_ = nullptr;

  // Listen extension's load, unload, uninstalled.
  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_MODEL_BUILDER_H_
