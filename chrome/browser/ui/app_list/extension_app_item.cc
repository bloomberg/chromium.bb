// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "grit/theme_resources.h"
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

ExtensionSorting* GetExtensionSorting(Profile* profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extension_prefs()->extension_sorting();
}

const color_utils::HSL shift = {-1, 0, 0.6};

}  // namespace

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const std::string& extension_id,
                                   AppListControllerDelegate* controller,
                                   const std::string& extension_name,
                                   const gfx::ImageSkia& installing_icon,
                                   bool is_platform_app)
    : ChromeAppListItem(TYPE_APP),
      profile_(profile),
      extension_id_(extension_id),
      controller_(controller),
      extension_name_(extension_name),
      installing_icon_(
          gfx::ImageSkiaOperations::CreateHSLShiftedImage(installing_icon,
                                                          shift)),
      is_platform_app_(is_platform_app) {
  Reload();
  GetExtensionSorting(profile_)->EnsureValidOrdinals(extension_id_,
                                                     syncer::StringOrdinal());
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
  set_app_id(extension_id_);
  if (is_installing) {
    SetTitleAndFullName(extension_name_, extension_name_);
    UpdateIcon();
    return;
  }
  SetTitleAndFullName(extension->short_name(), extension->name());
  LoadImage(extension);
}

syncer::StringOrdinal ExtensionAppItem::GetPageOrdinal() const {
  return GetExtensionSorting(profile_)->GetPageOrdinal(extension_id_);
}

syncer::StringOrdinal ExtensionAppItem::GetAppLaunchOrdinal() const {
  return GetExtensionSorting(profile_)->GetAppLaunchOrdinal(extension_id_);
}

void ExtensionAppItem::Move(const ExtensionAppItem* prev,
                            const ExtensionAppItem* next) {
  // Does nothing if no predecessor nor successor.
  if (!prev && !next)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->extension_prefs()->SetAppDraggedByUser(extension_id_);

  // Handles only predecessor or only successor case.
  if (!prev || !next) {
    syncer::StringOrdinal page = prev ? prev->GetPageOrdinal() :
                                        next->GetPageOrdinal();
    GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, page);
    service->OnExtensionMoved(extension_id_,
                              prev ? prev->extension_id() : std::string(),
                              next ? next->extension_id() : std::string());
    return;
  }

  // Handles both predecessor and successor are on the same page.
  syncer::StringOrdinal prev_page = prev->GetPageOrdinal();
  syncer::StringOrdinal next_page = next->GetPageOrdinal();
  if (prev_page.Equals(next_page)) {
    GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, prev_page);
    service->OnExtensionMoved(extension_id_,
                              prev->extension_id(),
                              next->extension_id());
    return;
  }

  // Otherwise, go with |next|. This is okay because app list does not split
  // page based ntp page ordinal.
  // TODO(xiyuan): Revisit this when implementing paging support.
  GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, prev_page);
  service->OnExtensionMoved(extension_id_,
                            prev->extension_id(),
                            std::string());
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
  const bool enabled = service->IsExtensionEnabledForLauncher(extension_id_);
  if (!enabled) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

  if (HasOverlay())
    icon = gfx::ImageSkia(new ShortcutOverlayImageSource(icon), icon.size());

  SetIcon(icon, !HasOverlay());
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
  if (service->IsExtensionEnabledForLauncher(extension_id_))
    return false;

  if (!extension_enable_flow_) {
    controller_->OnShowExtensionPrompt();

    extension_enable_flow_.reset(new ExtensionEnableFlow(
        profile_, extension_id_, this));
    extension_enable_flow_->StartForNativeWindow(
        controller_->GetAppListWindow());
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

  controller_->LaunchApp(profile_, extension, event_flags);
}

void ExtensionAppItem::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK(icon_.get() == image);
  UpdateIcon();
}

void ExtensionAppItem::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  controller_->OnCloseExtensionPrompt();

  // Automatically launch app after enabling.
  Launch(ui::EF_NONE);
}

void ExtensionAppItem::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  controller_->OnCloseExtensionPrompt();
}

void ExtensionAppItem::Activate(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (RunExtensionEnableFlow())
    return;

  CoreAppLauncherHandler::RecordAppListMainLaunch(extension);
  controller_->ActivateApp(profile_, extension, event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  if (!context_menu_) {
    context_menu_.reset(new app_list::AppContextMenu(
        this, profile_, extension_id_, controller_, is_platform_app_));
  }

  return context_menu_->GetMenuModel();
}

void ExtensionAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags);
}
