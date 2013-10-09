// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/image/image_skia.h"

using extensions::Extension;

namespace {

bool ShouldDisplayInAppLauncher(Profile* profile,
                                scoped_refptr<const Extension> app) {
  // If it's the web store, check the policy.
  bool blocked_by_policy =
      (app->id() == extension_misc::kWebStoreAppId ||
       app->id() == extension_misc::kEnterpriseWebStoreAppId) &&
      profile->GetPrefs()->GetBoolean(prefs::kHideWebStoreIcon);
  return app->ShouldDisplayInAppLauncher() && !blocked_by_policy;
}

}  // namespace

ExtensionAppModelBuilder::ExtensionAppModelBuilder(
    Profile* profile,
    app_list::AppListModel* model,
    AppListControllerDelegate* controller)
    : profile_(NULL),
      controller_(controller),
      model_(model),
      highlighted_app_pending_(false),
      tracker_(NULL) {
  SwitchProfile(profile);  // Builds the model.
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  OnShutdown();
}

void ExtensionAppModelBuilder::OnBeginExtensionInstall(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_app,
    bool is_platform_app) {
  if (!is_app)
    return;
  InsertApp(new ExtensionAppItem(profile_,
                                 extension_id,
                                 controller_,
                                 extension_name,
                                 installing_icon,
                                 is_platform_app));
  SetHighlightedApp(extension_id);
}

void ExtensionAppModelBuilder::OnDownloadProgress(
    const std::string& extension_id,
    int percent_downloaded) {
  ExtensionAppItem* item = GetExtensionAppItem(extension_id);
  if (!item)
    return;
  item->SetPercentDownloaded(percent_downloaded);
}

void ExtensionAppModelBuilder::OnInstallFailure(
    const std::string& extension_id) {
  // Do nothing, item will be disabled
}

void ExtensionAppModelBuilder::OnExtensionLoaded(const Extension* extension) {
  if (!extension->ShouldDisplayInAppLauncher())
    return;

  ExtensionAppItem* existing_item = GetExtensionAppItem(extension->id());
  if (existing_item) {
    existing_item->Reload();
    return;
  }

  InsertApp(new ExtensionAppItem(profile_,
                                 extension->id(),
                                 controller_,
                                 "",
                                 gfx::ImageSkia(),
                                 extension->is_platform_app()));
  UpdateHighlight();
}

void ExtensionAppModelBuilder::OnExtensionUnloaded(const Extension* extension) {
  ExtensionAppItem* item = GetExtensionAppItem(extension->id());
  if (!item)
    return;
  item->UpdateIcon();
}

void ExtensionAppModelBuilder::OnExtensionUninstalled(
    const Extension* extension) {
  model_->DeleteItem(extension->id());
}

void ExtensionAppModelBuilder::OnAppsReordered() {
  // Do nothing; App List order does not track extensions order.
}

void ExtensionAppModelBuilder::OnAppInstalledToAppList(
    const std::string& extension_id) {
  SetHighlightedApp(extension_id);
}

void ExtensionAppModelBuilder::OnShutdown() {
  if (tracker_) {
    tracker_->RemoveObserver(this);
    tracker_ = NULL;
  }
}

void ExtensionAppModelBuilder::AddApps(const ExtensionSet* extensions,
                                       ExtensionAppList* apps) {
  for (ExtensionSet::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if (ShouldDisplayInAppLauncher(profile_, *app))
      apps->push_back(new ExtensionAppItem(profile_,
                                           (*app)->id(),
                                           controller_,
                                           "",
                                           gfx::ImageSkia(),
                                           (*app)->is_platform_app()));
  }
}

void ExtensionAppModelBuilder::SwitchProfile(Profile* profile) {
  if (profile_ == profile)
    return;
  profile_ = profile;

  // Delete any extension apps.
  app_list::AppListModel::Apps* app_list = model_->apps();
  for (size_t i = 0; i < app_list->item_count(); ++i) {
    app_list::AppListItemModel* item = app_list->GetItemAt(i);
    if (item->GetAppType() == ExtensionAppItem::kAppType)
      app_list->DeleteAt(i);
  }

  if (tracker_)
    tracker_->RemoveObserver(this);

  tracker_ = extensions::InstallTrackerFactory::GetForProfile(profile_);

  PopulateApps();
  UpdateHighlight();

  // Start observing after model is built.
  tracker_->AddObserver(this);
}

void ExtensionAppModelBuilder::PopulateApps() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;

  ExtensionAppList apps;
  AddApps(service->extensions(), &apps);
  AddApps(service->disabled_extensions(), &apps);
  AddApps(service->terminated_extensions(), &apps);

  if (apps.empty())
    return;

  for (size_t i = 0; i < apps.size(); ++i)
    InsertApp(apps[i]);
}

void ExtensionAppModelBuilder::InsertApp(ExtensionAppItem* app) {
  model_->AddItem(app);
}

void ExtensionAppModelBuilder::SetHighlightedApp(
    const std::string& extension_id) {
  if (extension_id == highlight_app_id_)
    return;
  ExtensionAppItem* old_app = GetExtensionAppItem(highlight_app_id_);
  if (old_app)
    old_app->SetHighlighted(false);
  highlight_app_id_ = extension_id;
  ExtensionAppItem* new_app = GetExtensionAppItem(highlight_app_id_);
  highlighted_app_pending_ = !new_app;
  if (new_app)
    new_app->SetHighlighted(true);
}

ExtensionAppItem* ExtensionAppModelBuilder::GetExtensionAppItem(
    const std::string& extension_id) {
  app_list::AppListItemModel* item = model_->FindItem(extension_id);
  LOG_IF(ERROR, item &&
         item->GetAppType() != ExtensionAppItem::kAppType)
      << "App Item matching id: " << extension_id
      << " has incorrect type: '" << item->GetAppType() << "'";
  return static_cast<ExtensionAppItem*>(item);
}

void ExtensionAppModelBuilder::UpdateHighlight() {
  DCHECK(model_);
  if (!highlighted_app_pending_ || highlight_app_id_.empty())
    return;
  ExtensionAppItem* item = GetExtensionAppItem(highlight_app_id_);
  if (!item)
    return;
  item->SetHighlighted(true);
  highlighted_app_pending_ = false;
}
