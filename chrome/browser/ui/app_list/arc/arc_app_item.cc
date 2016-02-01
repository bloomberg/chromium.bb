// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_item.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_sorting.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/image/image_skia.h"

// static
const char ArcAppItem::kItemType[] = "ArcAppItem";

ArcAppItem::ArcAppItem(
    Profile* profile,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name,
    bool ready)
    : ChromeAppListItem(profile, id),
      ready_(ready) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  arc_app_icon_.reset(new ArcAppIcon(profile,
                                     id,
                                     app_list::kGridIconDimension,
                                     this));

  SetName(name);
  UpdateIcon();
  if (sync_item && sync_item->item_ordinal.IsValid())
    UpdateFromSync(sync_item);
  else
    UpdatePositionFromOrdering();
}

ArcAppItem::~ArcAppItem() {
}

const char* ArcAppItem::GetItemType() const {
  return ArcAppItem::kItemType;
}

void ArcAppItem::Activate(int event_flags) {
  if (!ready()) {
    VLOG(2) << "Cannot launch not-ready app:" << id() << ".";
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id());
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app:" << id() << ".";
    return;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << id() << ".";
    return;
  }
  arc::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << id() << ".";
    return;
  }

  app_instance->LaunchApp(app_info->package_name, app_info->activity);

  // Manually close app_list view because focus is not changed on ARC app start,
  // and current view remains active.
  GetController()->DismissView();
}

void ArcAppItem::SetReady(bool ready) {
  if (ready_ == ready)
    return;

  ready_ = ready;
  UpdateIcon();
}

void ArcAppItem::SetName(const std::string& name) {
  SetNameAndShortName(name, name);
}

void ArcAppItem::UpdateIcon() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gfx::ImageSkia icon = arc_app_icon_->image_skia();
  if (!ready_)
    icon = CreateDisabledIcon(icon);

  SetIcon(icon);
}

void ArcAppItem::UpdatePositionFromOrdering() {
  // There is an ExtensionAppItem that uses extension::AppSorting to order
  // its element. There is no commonly available sorting mechanism for app
  // ordering so use the only one available from extension subsystem.
  // Page is the earliest non-full page.
  const syncer::StringOrdinal& page =
      GetAppSorting()->GetNaturalAppPageOrdinal();
  // And get next available pos in this page.
  const syncer::StringOrdinal& pos =
     GetAppSorting()->CreateNextAppLaunchOrdinal(page);
  set_position(pos);
}

void ArcAppItem::OnIconUpdated() {
  UpdateIcon();
}
