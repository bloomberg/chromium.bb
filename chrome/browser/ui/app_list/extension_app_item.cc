// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
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

const color_utils::HSL shift = {-1, 0, 0.6};

}  // namespace

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const std::string& extension_id,
                                   const std::string& extension_name,
                                   const gfx::ImageSkia& installing_icon,
                                   bool is_platform_app)
    : app_list::AppListItemModel(extension_id),
      profile_(profile),
      extension_id_(extension_id),
      extension_enable_flow_controller_(NULL),
      extension_name_(extension_name),
      installing_icon_(
          gfx::ImageSkiaOperations::CreateHSLShiftedImage(installing_icon,
                                                          shift)),
      is_platform_app_(is_platform_app) {
  Reload();
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

  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const bool enabled = extension_util::IsAppLaunchable(extension_id_, service);
  if (!enabled) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

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
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (extension_util::IsAppLaunchableWithoutEnabling(extension_id_, service))
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

  content::RecordAction(content::UserMetricsAction("AppList_ClickOnApp"));
  CoreAppLauncherHandler::RecordAppListMainLaunch(extension);
  GetController()->ActivateApp(profile_,
                               extension,
                               AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                               event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  if (!context_menu_) {
    context_menu_.reset(new app_list::AppContextMenu(
        this, profile_, extension_id_, GetController(),
        is_platform_app_, false));
  }

  return context_menu_->GetMenuModel();
}

// static
const char ExtensionAppItem::kAppType[] = "ExtensionAppItem";

const char* ExtensionAppItem::GetAppType() const {
  return ExtensionAppItem::kAppType;
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
