// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_mash_shelf_controller.h"

#include "chrome/browser/extensions/extension_app_icon_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

class ChromeShelfItemDelegate : public ash::mojom::ShelfItemDelegate {
 public:
  explicit ChromeShelfItemDelegate(const std::string& app_id,
                                   ChromeMashShelfController* controller)
      : app_id_(app_id),
        item_delegate_binding_(this),
        controller_(controller) {}
  ~ChromeShelfItemDelegate() override {}

  ash::mojom::ShelfItemDelegateAssociatedPtrInfo CreateInterfacePtrInfoAndBind(
      mojo::AssociatedGroup* associated_group) {
    DCHECK(!item_delegate_binding_.is_bound());
    ash::mojom::ShelfItemDelegateAssociatedPtrInfo ptr_info;
    item_delegate_binding_.Bind(&ptr_info, associated_group);
    return ptr_info;
  }

 private:
  // ash::mojom::ShelfItemDelegate:
  void LaunchItem() override { controller_->LaunchItem(app_id_); }
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override {
    NOTIMPLEMENTED();
  }
  void ItemPinned() override { NOTIMPLEMENTED(); }
  void ItemUnpinned() override { NOTIMPLEMENTED(); }
  void ItemReordered(uint32_t order) override { NOTIMPLEMENTED(); }

  std::string app_id_;
  mojo::AssociatedBinding<ash::mojom::ShelfItemDelegate> item_delegate_binding_;

  // Not owned.
  ChromeMashShelfController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShelfItemDelegate);
};

ChromeMashShelfController::ChromeMashShelfController()
    : helper_(ProfileManager::GetActiveUserProfile()),
      observer_binding_(this) {
  Init();
}

ChromeMashShelfController::~ChromeMashShelfController() {}

void ChromeMashShelfController::LaunchItem(const std::string& app_id) {
  helper_.LaunchApp(app_id, ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE);
}

void ChromeMashShelfController::Init() {
  shell::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->ConnectToInterface("service:ash", &shelf_controller_);

  // Initialize shelf alignment and auto-hide behavior from preferences.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    OnShelfCreated(display.id());

  // TODO(skuhne): The AppIconLoaderImpl has the same problem. Each loaded
  // image is associated with a profile (its loader requires the profile).
  // Since icon size changes are possible, the icon could be requested to be
  // reloaded. However - having it not multi profile aware would cause problems
  // if the icon cache gets deleted upon user switch.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<AppIconLoader> extension_app_icon_loader(
      new extensions::ExtensionAppIconLoader(
          profile, extension_misc::EXTENSION_ICON_SMALL, this));
  app_icon_loaders_.push_back(std::move(extension_app_icon_loader));

  if (arc::ArcAuthService::IsAllowedForProfile(profile)) {
    std::unique_ptr<AppIconLoader> arc_app_icon_loader(new ArcAppIconLoader(
        profile, extension_misc::EXTENSION_ICON_SMALL, this));
    app_icon_loaders_.push_back(std::move(arc_app_icon_loader));
  }

  PinAppsFromPrefs();

  // Start observing the shelf now that it has been initialized.
  ash::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
  observer_binding_.Bind(&ptr_info, shelf_controller_.associated_group());
  shelf_controller_->AddObserver(std::move(ptr_info));
}

void ChromeMashShelfController::PinAppsFromPrefs() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::vector<std::string> pinned_apps =
      ash::launcher::GetPinnedAppsFromPrefs(profile->GetPrefs(), &helper_);

  for (const auto& app : pinned_apps) {
    if (app == ash::launcher::kPinnedAppsPlaceholder)
      continue;

    ash::mojom::ShelfItemPtr item(ash::mojom::ShelfItem::New());
    item->app_id = app;
    item->app_title = mojo::String::From(helper_.GetAppTitle(profile, app));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Image& image = rb.GetImageNamed(IDR_APP_DEFAULT_ICON);
    item->image = *image.ToSkBitmap();
    std::unique_ptr<ChromeShelfItemDelegate> delegate(
        new ChromeShelfItemDelegate(app, this));
    shelf_controller_->PinItem(std::move(item),
                               delegate->CreateInterfacePtrInfoAndBind(
                                   shelf_controller_.associated_group()));
    app_id_to_item_delegate_.insert(std::make_pair(app, std::move(delegate)));

    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app);
    if (app_icon_loader) {
      app_icon_loader->FetchImage(app);
      app_icon_loader->UpdateImage(app);
    }
  }
}

AppIconLoader* ChromeMashShelfController::GetAppIconLoaderForApp(
    const std::string& app_id) {
  for (const auto& app_icon_loader : app_icon_loaders_) {
    if (app_icon_loader->CanLoadImageForApp(app_id))
      return app_icon_loader.get();
  }

  return nullptr;
}

void ChromeMashShelfController::OnShelfCreated(int64_t display_id) {
  // The pref helper functions return default values for invalid display ids.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  shelf_controller_->SetAlignment(
      ash::launcher::GetShelfAlignmentPref(profile->GetPrefs(), display_id),
      display_id);
  shelf_controller_->SetAutoHideBehavior(
      ash::launcher::GetShelfAutoHideBehaviorPref(profile->GetPrefs(),
                                                  display_id),
      display_id);
}

void ChromeMashShelfController::OnAlignmentChanged(
    ash::ShelfAlignment alignment,
    int64_t display_id) {
  // The locked alignment is set temporarily and not saved to preferences.
  if (alignment == ash::SHELF_ALIGNMENT_BOTTOM_LOCKED)
    return;
  // This will uselessly store a preference value for invalid display ids.
  ash::launcher::SetShelfAlignmentPref(
      ProfileManager::GetActiveUserProfile()->GetPrefs(), display_id,
      alignment);
}

void ChromeMashShelfController::OnAutoHideBehaviorChanged(
    ash::ShelfAutoHideBehavior auto_hide,
    int64_t display_id) {
  // This will uselessly store a preference value for invalid display ids.
  ash::launcher::SetShelfAutoHideBehaviorPref(
      ProfileManager::GetActiveUserProfile()->GetPrefs(), display_id,
      auto_hide);
}

void ChromeMashShelfController::OnAppImageUpdated(const std::string& app_id,
                                                  const gfx::ImageSkia& image) {
  shelf_controller_->SetItemImage(app_id, *image.bitmap());
}
