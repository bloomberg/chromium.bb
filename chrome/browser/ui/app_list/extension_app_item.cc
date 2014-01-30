// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/theme_resources.h"
#include "sync/api/string_ordinal.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;

namespace {

// Overlays a shortcut icon over the bottom left corner of a given image.
class ShortcutOverlayImageSource : public gfx::CanvasImageSource {
 public:
  explicit ShortcutOverlayImageSource(const gfx::ImageSkia& icon)
      : gfx::CanvasImageSource(icon.size(), false),
        icon_(icon) {
  }
  virtual ~ShortcutOverlayImageSource() {}

 private:
  // gfx::CanvasImageSource overrides:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawImageInt(icon_, 0, 0);

    // Draw the overlay in the bottom left corner of the icon.
    const gfx::ImageSkia& overlay = *ui::ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_APP_LIST_TAB_OVERLAY);
    canvas->DrawImageInt(overlay, 0, icon_.height() - overlay.height());
  }

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutOverlayImageSource);
};

extensions::AppSorting* GetAppSorting(Profile* profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extension_prefs()->app_sorting();
}

// Returns the given |icon| with rounded corners.
gfx::ImageSkia GetRoundedIcon(gfx::ImageSkia icon) {
  // The radius used to round the app icon.
  const size_t kRoundingRadius = 2;

  scoped_ptr<SkCanvas> canvas(
      skia::CreateBitmapCanvas(icon.width(), icon.height(), false));
  DCHECK(canvas);
  canvas->drawBitmap(*icon.bitmap(), 0, 0);

  scoped_ptr<SkCanvas> masking_canvas(
      skia::CreateBitmapCanvas(icon.width(), icon.height(), false));
  DCHECK(masking_canvas);

  SkPaint opaque_paint;
  opaque_paint.setColor(SK_ColorWHITE);
  opaque_paint.setFlags(SkPaint::kAntiAlias_Flag);
  masking_canvas->drawRoundRect(
    SkRect::MakeWH(icon.width(), icon.height()),
    kRoundingRadius, kRoundingRadius, opaque_paint);

  SkPaint masking_paint;
  masking_paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
  canvas->drawBitmap(
      masking_canvas->getDevice()->accessBitmap(false), 0, 0, &masking_paint);

  return gfx::ImageSkia::CreateFrom1xBitmap(
      canvas->getDevice()->accessBitmap(false));
}

const color_utils::HSL shift = {-1, 0, 0.6};

}  // namespace

ExtensionAppItem::ExtensionAppItem(
    Profile* profile,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_platform_app)
    : app_list::AppListItem(extension_id),
      profile_(profile),
      extension_id_(extension_id),
      extension_enable_flow_controller_(NULL),
      extension_name_(extension_name),
      installing_icon_(
          gfx::ImageSkiaOperations::CreateHSLShiftedImage(installing_icon,
                                                          shift)),
      is_platform_app_(is_platform_app) {
  Reload();
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    // An existing synced position exists, use that.
    set_position(sync_item->item_ordinal);
    if (title().empty())
      SetTitleAndFullName(sync_item->item_name, sync_item->item_name);
    return;
  }
  GetAppSorting(profile_)->EnsureValidOrdinals(extension_id_,
                                               syncer::StringOrdinal());
  UpdatePositionFromExtensionOrdering();
}

ExtensionAppItem::~ExtensionAppItem() {
}

bool ExtensionAppItem::HasOverlay() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return !is_platform_app_ && extension_id_ != extension_misc::kChromeAppId;
#endif
}

void ExtensionAppItem::Reload() {
  const Extension* extension = GetExtension();
  bool is_installing = !extension;
  SetIsInstalling(is_installing);
  if (is_installing) {
    SetTitleAndFullName(extension_name_, extension_name_);
    UpdateIcon();
    return;
  }
  SetTitleAndFullName(extension->short_name(), extension->name());
  LoadImage(extension);
}

void ExtensionAppItem::UpdateIcon() {
  if (!GetExtension()) {
    gfx::ImageSkia icon = installing_icon_;
    if (HasOverlay())
      icon = gfx::ImageSkia(new ShortcutOverlayImageSource(icon), icon.size());
    SetIcon(icon, !HasOverlay());
    return;
  }
  gfx::ImageSkia icon = icon_->image_skia();

  if (!extensions::util::IsAppLaunchable(extension_id_, profile_)) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

  if (GetExtension()->from_bookmark())
    icon = GetRoundedIcon(icon);

  if (HasOverlay())
    icon = gfx::ImageSkia(new ShortcutOverlayImageSource(icon), icon.size());

  SetIcon(icon, !HasOverlay());
}

void ExtensionAppItem::Move(const ExtensionAppItem* prev,
                            const ExtensionAppItem* next) {
  if (!prev && !next)
    return;  // No reordering necessary

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::AppSorting* sorting = service->extension_prefs()->app_sorting();

  syncer::StringOrdinal page;
  std::string prev_id, next_id;
  if (!prev) {
    next_id = next->extension_id();
    page = sorting->GetPageOrdinal(next_id);
  } else if (!next) {
    prev_id = prev->extension_id();
    page = sorting->GetPageOrdinal(prev_id);
  } else {
    prev_id = prev->extension_id();
    page = sorting->GetPageOrdinal(prev_id);
    // Only set |next_id| if on the same page, otherwise just insert after prev.
    if (page.Equals(sorting->GetPageOrdinal(next->extension_id())))
      next_id = next->extension_id();
  }
  service->extension_prefs()->SetAppDraggedByUser(extension_id_);
  sorting->SetPageOrdinal(extension_id_, page);
  service->OnExtensionMoved(extension_id_, prev_id, next_id);
  UpdatePositionFromExtensionOrdering();
}

const Extension* ExtensionAppItem::GetExtension() const {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetInstalledExtension(extension_id_);
  return extension;
}

void ExtensionAppItem::LoadImage(const Extension* extension) {
  icon_.reset(new extensions::IconImage(
      profile_,
      extension,
      extensions::IconsInfo::GetIcons(extension),
      extension_misc::EXTENSION_ICON_MEDIUM,
      extensions::IconsInfo::GetDefaultAppIcon(),
      this));
  UpdateIcon();
}

bool ExtensionAppItem::RunExtensionEnableFlow() {
  if (extensions::util::IsAppLaunchableWithoutEnabling(extension_id_, profile_))
    return false;

  if (!extension_enable_flow_) {
    extension_enable_flow_controller_ = GetController();
    extension_enable_flow_controller_->OnShowExtensionPrompt();

    extension_enable_flow_.reset(new ExtensionEnableFlow(
        profile_, extension_id_, this));
    extension_enable_flow_->StartForNativeWindow(
        extension_enable_flow_controller_->GetAppListWindow());
  }
  return true;
}

void ExtensionAppItem::Launch(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (RunExtensionEnableFlow())
    return;

  GetController()->LaunchApp(profile_,
                             extension,
                             AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                             event_flags);
}

void ExtensionAppItem::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK(icon_.get() == image);
  UpdateIcon();
}

void ExtensionAppItem::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  extension_enable_flow_controller_->OnCloseExtensionPrompt();
  extension_enable_flow_controller_ = NULL;

  // Automatically launch app after enabling.
  Launch(ui::EF_NONE);
}

void ExtensionAppItem::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  extension_enable_flow_controller_->OnCloseExtensionPrompt();
  extension_enable_flow_controller_ = NULL;
}

void ExtensionAppItem::Activate(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (RunExtensionEnableFlow())
    return;

  content::RecordAction(base::UserMetricsAction("AppList_ClickOnApp"));
  CoreAppLauncherHandler::RecordAppListMainLaunch(extension);
  GetController()->ActivateApp(profile_,
                               extension,
                               AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                               event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  context_menu_.reset(new app_list::AppContextMenu(
      this, profile_, extension_id_, GetController()));
  context_menu_->set_is_platform_app(is_platform_app_);
  if (IsInFolder())
    context_menu_->set_is_in_folder(true);
  return context_menu_->GetMenuModel();
}

// static
const char ExtensionAppItem::kItemType[] = "ExtensionAppItem";

const char* ExtensionAppItem::GetItemType() const {
  return ExtensionAppItem::kItemType;
}

void ExtensionAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags);
}

void ExtensionAppItem::UpdatePositionFromExtensionOrdering() {
  const syncer::StringOrdinal& page =
      GetAppSorting(profile_)->GetPageOrdinal(extension_id_);
  const syncer::StringOrdinal& launch =
     GetAppSorting(profile_)->GetAppLaunchOrdinal(extension_id_);
  set_position(syncer::StringOrdinal(
      page.ToInternalValue() + launch.ToInternalValue()));
}

AppListControllerDelegate* ExtensionAppItem::GetController() {
  return AppListService::Get(chrome::GetActiveDesktop())->
      GetControllerDelegate();
}
