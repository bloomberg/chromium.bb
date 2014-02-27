// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
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
    AppListControllerDelegate* controller)
    : service_(NULL),
      profile_(NULL),
      controller_(controller),
      model_(NULL),
      highlighted_app_pending_(false),
      tracker_(NULL) {
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  OnShutdown();
  model_->item_list()->RemoveObserver(this);
}

void ExtensionAppModelBuilder::InitializeWithService(
    app_list::AppListSyncableService* service) {
  DCHECK(!service_ && !profile_);
  model_ = service->model();
  model_->item_list()->AddObserver(this);
  service_ = service;
  profile_ = service->profile();
  InitializePrefChangeRegistrar();

  BuildModel();
}

void ExtensionAppModelBuilder::InitializeWithProfile(
    Profile* profile,
    app_list::AppListModel* model) {
  DCHECK(!service_ && !profile_);
  model_ = model;
  model_->item_list()->AddObserver(this);
  profile_ = profile;
  InitializePrefChangeRegistrar();

  BuildModel();
}

void ExtensionAppModelBuilder::InitializePrefChangeRegistrar() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps))
    return;

  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  extension_pref_change_registrar_.Init(
      client->GetPrefServiceForContext(profile_));
  extension_pref_change_registrar_.Add(
    extensions::pref_names::kExtensions,
    base::Bind(&ExtensionAppModelBuilder::OnExtensionPreferenceChanged,
               base::Unretained(this)));
}

void ExtensionAppModelBuilder::OnExtensionPreferenceChanged() {
  // TODO(calamity): analyze the performance impact of doing this every
  // extension pref change.
  app_list::AppListItemList* item_list = model_->item_list();
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    app_list::AppListItem* item = item_list->item_at(i);
    if (item->GetItemType() != ExtensionAppItem::kItemType)
      continue;

    static_cast<ExtensionAppItem*>(item)->UpdateIconOverlay();
  }
}

void ExtensionAppModelBuilder::OnBeginExtensionInstall(
    const ExtensionInstallParams& params) {
  if (!params.is_app || params.is_ephemeral)
    return;

  DVLOG(2) << service_ << ": OnBeginExtensionInstall: "
           << params.extension_id.substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(params.extension_id);
  if (existing_item) {
    existing_item->SetIsInstalling(true);
    return;
  }
  InsertApp(CreateAppItem(params.extension_id,
                          params.extension_name,
                          params.installing_icon,
                          params.is_platform_app));
  SetHighlightedApp(params.extension_id);
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
  model_->DeleteItem(extension_id);
}

void ExtensionAppModelBuilder::OnExtensionLoaded(const Extension* extension) {
  if (!extension->ShouldDisplayInAppLauncher())
    return;

  DVLOG(2) << service_ << ": OnExtensionLoaded: "
           << extension->id().substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(extension->id());
  if (existing_item) {
    existing_item->Reload();
    return;
  }

  InsertApp(CreateAppItem(extension->id(),
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
  if (service_) {
    DVLOG(2) << service_ << ": OnExtensionUninstalled: "
             << extension->id().substr(0, 8);
    service_->RemoveItem(extension->id());
    return;
  }
  model_->DeleteItem(extension->id());
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

scoped_ptr<ExtensionAppItem> ExtensionAppModelBuilder::CreateAppItem(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_platform_app) {
  const app_list::AppListSyncableService::SyncItem* sync_item =
      service_ ? service_->GetSyncItem(extension_id) : NULL;
  return make_scoped_ptr(new ExtensionAppItem(profile_,
                                              sync_item,
                                              extension_id,
                                              extension_name,
                                              installing_icon,
                                              is_platform_app));
}

void ExtensionAppModelBuilder::BuildModel() {
  DCHECK(!tracker_);
  tracker_ = controller_->GetInstallTrackerFor(profile_);

  PopulateApps();
  UpdateHighlight();

  // Start observing after model is built.
  if (tracker_)
    tracker_->AddObserver(this);
}

void ExtensionAppModelBuilder::PopulateApps() {
  extensions::ExtensionSet extensions;
  controller_->GetApps(profile_, &extensions);

  for (extensions::ExtensionSet::const_iterator app = extensions.begin();
       app != extensions.end(); ++app) {
    if (!ShouldDisplayInAppLauncher(profile_, *app))
      continue;
    InsertApp(CreateAppItem((*app)->id(),
                            "",
                            gfx::ImageSkia(),
                            (*app)->is_platform_app()));
  }
}

void ExtensionAppModelBuilder::InsertApp(scoped_ptr<ExtensionAppItem> app) {
  if (service_) {
    service_->AddItem(app.PassAs<app_list::AppListItem>());
    return;
  }
  model_->AddItem(app.PassAs<app_list::AppListItem>());
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
  app_list::AppListItem* item =
      model_->item_list()->FindItem(extension_id);
  LOG_IF(ERROR, item &&
         item->GetItemType() != ExtensionAppItem::kItemType)
      << "App Item matching id: " << extension_id
      << " has incorrect type: '" << item->GetItemType() << "'";
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

void ExtensionAppModelBuilder::OnListItemMoved(size_t from_index,
                                               size_t to_index,
                                               app_list::AppListItem* item) {
  // This will get called from AppListItemList::ListItemMoved after
  // set_position is called for the item.
  app_list::AppListItemList* item_list = model_->item_list();
  if (item->GetItemType() != ExtensionAppItem::kItemType)
    return;

  if (service_)
    return;

  ExtensionAppItem* prev = NULL;
  for (size_t idx = to_index; idx > 0; --idx) {
    app_list::AppListItem* item = item_list->item_at(idx - 1);
    if (item->GetItemType() == ExtensionAppItem::kItemType) {
      prev = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  ExtensionAppItem* next = NULL;
  for (size_t idx = to_index; idx < item_list->item_count() - 1; ++idx) {
    app_list::AppListItem* item = item_list->item_at(idx + 1);
    if (item->GetItemType() == ExtensionAppItem::kItemType) {
      next = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  // item->Move will call set_position, overriding the item's position.
  if (prev || next)
    static_cast<ExtensionAppItem*>(item)->Move(prev, next);
}
