// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_item.h"

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

gfx::ImageSkia CreateDisabledIcon(const gfx::ImageSkia& icon) {
  const color_utils::HSL shift = {-1, 0, 0.6};
  return gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
}

extensions::AppSorting* GetAppSorting(content::BrowserContext* context) {
  return extensions::ExtensionPrefs::Get(context)->app_sorting();
}

}  // namespace

// static
const char ArcAppItem::kItemType[] = "ArcAppItem";

ArcAppItem::ArcAppItem(
    content::BrowserContext* context,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name,
    bool ready)
    : app_list::AppListItem(id),
      context_(context),
      ready_(ready) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  arc_app_icon_.reset(new ArcAppIcon(context_,
                                     id,
                                     app_list::kGridIconDimension,
                                     this));

  SetName(name);
  UpdateIcon();

  if (sync_item && sync_item->item_ordinal.IsValid()) {
    // An existing synced position exists, use that.
    set_position(sync_item->item_ordinal);
  } else {
    // There is an ExtensionAppItem that uses extension::AppSorting to order
    // its element. There is no commonly available sorting mechanism for app
    // ordering so use the only one available from extension subsystem.
    // Page is is the earliest non-full page.
    const syncer::StringOrdinal& page =
        GetAppSorting(context_)->GetNaturalAppPageOrdinal();
    // And get next available pos in this page.
    const syncer::StringOrdinal& pos =
       GetAppSorting(context_)->CreateNextAppLaunchOrdinal(page);
    set_position(pos);
  }
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

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id());
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app:" << id() << ".";
    return;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service ||
      bridge_service->state() != arc::ArcBridgeService::State::READY) {
    VLOG(2) << "Cannot launch app: " << app_info->package
            << ". Bridge service is not ready.";
    return;
  }

  bridge_service->LaunchApp(app_info->package, app_info->activity);
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

void ArcAppItem::OnIconUpdated() {
  UpdateIcon();
}
