// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
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
  model_->item_list()->AddObserver(this);
  SwitchProfile(profile);  // Builds the model.
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  OnShutdown();
  model_->item_list()->RemoveObserver(this);
}

void ExtensionAppModelBuilder::OnBeginExtensionInstall(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_app,
    bool is_platform_app) {
  if (!is_app)
    return;

  ExtensionAppItem* existing_item = GetExtensionAppItem(extension_id);
  if (existing_item) {
    existing_item->SetIsInstalling(true);
    return;
  }

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
  model_->item_list()->DeleteItem(extension->id());
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
  model_->item_list()->DeleteItemsByType(ExtensionAppItem::kAppType);

  if (tracker_)
    tracker_->RemoveObserver(this);

  tracker_ = controller_->GetInstallTrackerFor(profile_);

  PopulateApps();
  UpdateHighlight();

  // Start observing after model is built.
  if (tracker_)
    tracker_->AddObserver(this);
}

void ExtensionAppModelBuilder::PopulateApps() {
  ExtensionSet extensions;
  controller_->GetApps(profile_, &extensions);
  ExtensionAppList apps;
  AddApps(&extensions, &apps);

  if (apps.empty())
    return;

  for (size_t i = 0; i < apps.size(); ++i)
    InsertApp(apps[i]);
}

void ExtensionAppModelBuilder::InsertApp(ExtensionAppItem* app) {
  model_->item_list()->AddItem(app);
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
  app_list::AppListItemModel* item =
      model_->item_list()->FindItem(extension_id);
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

void ExtensionAppModelBuilder::OnListItemMoved(
    size_t from_index,
    size_t to_index,
    app_list::AppListItemModel* item) {
  // This will get called from AppListItemList::ListItemMoved after
  // set_position is called for the item.
  app_list::AppListItemList* item_list = model_->item_list();
  if (item->GetAppType() != ExtensionAppItem::kAppType)
    return;

  ExtensionAppItem* prev = NULL;
  for (size_t idx = to_index; idx > 0; --idx) {
    app_list::AppListItemModel* item = item_list->item_at(idx - 1);
    if (item->GetAppType() == ExtensionAppItem::kAppType) {
      prev = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  ExtensionAppItem* next = NULL;
  for (size_t idx = to_index; idx < item_list->item_count() - 1; ++idx) {
    app_list::AppListItemModel* item = item_list->item_at(idx + 1);
    if (item->GetAppType() == ExtensionAppItem::kAppType) {
      next = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  // item->Move will call set_position, overriding the item's position.
  if (prev || next)
    static_cast<ExtensionAppItem*>(item)->Move(prev, next);
}
