// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_mash_shelf_controller.h"

#include "chrome/browser/extensions/extension_app_icon_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/common/mojo_shell_connection.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/connector.h"
#include "skia/public/type_converters.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#endif

class ChromeShelfItemDelegate : public mash::shelf::mojom::ShelfItemDelegate {
 public:
  explicit ChromeShelfItemDelegate(const std::string& app_id)
      : app_id_(app_id), item_delegate_binding_(this) {}
  ~ChromeShelfItemDelegate() override {}

  mash::shelf::mojom::ShelfItemDelegateAssociatedPtrInfo
  CreateInterfacePtrInfoAndBind(mojo::AssociatedGroup* associated_group) {
    DCHECK(!item_delegate_binding_.is_bound());
    mash::shelf::mojom::ShelfItemDelegateAssociatedPtrInfo ptr_info;
    item_delegate_binding_.Bind(&ptr_info, associated_group);
    return ptr_info;
  }

 private:
  // mash::shelf::mojom::ShelfItemDelegate:
  void LaunchItem() override {
    ChromeMashShelfController::instance()->LaunchItem(app_id_);
  }
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override {
    NOTIMPLEMENTED();
  }
  void ItemPinned() override { NOTIMPLEMENTED(); }
  void ItemUnpinned() override { NOTIMPLEMENTED(); }
  void ItemReordered(uint32_t order) override { NOTIMPLEMENTED(); }

  std::string app_id_;
  mojo::AssociatedBinding<mash::shelf::mojom::ShelfItemDelegate>
      item_delegate_binding_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShelfItemDelegate);
};

// static
ChromeMashShelfController* ChromeMashShelfController::instance_ = nullptr;

ChromeMashShelfController::~ChromeMashShelfController() {}

// static
ChromeMashShelfController* ChromeMashShelfController::CreateInstance() {
  DCHECK(!instance_);
  instance_ = new ChromeMashShelfController();
  instance_->Init();
  return instance_;
}

void ChromeMashShelfController::LaunchItem(const std::string& app_id) {
  helper_.LaunchApp(app_id, ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE);
}

ChromeMashShelfController::ChromeMashShelfController()
    : helper_(ProfileManager::GetActiveUserProfile()),
      observer_binding_(this) {}

void ChromeMashShelfController::Init() {
  shell::Connector* connector =
      content::MojoShellConnection::Get()->GetConnector();
  connector->ConnectToInterface("mojo:ash_sysui", &shelf_controller_);

  // Set shelf alignment and auto-hide behavior from preferences.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  shelf_controller_->SetAlignment(static_cast<mash::shelf::mojom::Alignment>(
      ash::GetShelfAlignmentPref(profile->GetPrefs(), display_id)));
  shelf_controller_->SetAutoHideBehavior(
      static_cast<mash::shelf::mojom::AutoHideBehavior>(
          ash::GetShelfAutoHideBehaviorPref(profile->GetPrefs(), display_id)));

  // TODO(skuhne): The AppIconLoaderImpl has the same problem. Each loaded
  // image is associated with a profile (its loader requires the profile).
  // Since icon size changes are possible, the icon could be requested to be
  // reloaded. However - having it not multi profile aware would cause problems
  // if the icon cache gets deleted upon user switch.
  std::unique_ptr<AppIconLoader> extension_app_icon_loader(
      new extensions::ExtensionAppIconLoader(
          profile, extension_misc::EXTENSION_ICON_SMALL, this));
  app_icon_loaders_.push_back(std::move(extension_app_icon_loader));

#if defined(OS_CHROMEOS)
  std::unique_ptr<AppIconLoader> arc_app_icon_loader(new ArcAppIconLoader(
      profile, extension_misc::EXTENSION_ICON_SMALL, this));
  app_icon_loaders_.push_back(std::move(arc_app_icon_loader));
#endif

  PinAppsFromPrefs();

  // Start observing the shelf now that it has been initialized.
  mash::shelf::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
  observer_binding_.Bind(&ptr_info, shelf_controller_.associated_group());
  shelf_controller_->AddObserver(std::move(ptr_info));
}

void ChromeMashShelfController::PinAppsFromPrefs() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::vector<std::string> pinned_apps =
      ash::GetPinnedAppsFromPrefs(profile->GetPrefs(), &helper_);

  for (const auto& app : pinned_apps) {
    if (app == ash::kPinnedAppsPlaceholder)
      continue;

    mash::shelf::mojom::ShelfItemPtr item(mash::shelf::mojom::ShelfItem::New());
    item->app_id = app;
    item->app_title = mojo::String::From(helper_.GetAppTitle(profile, app));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Image& image = rb.GetImageNamed(IDR_APP_DEFAULT_ICON);
    item->image = skia::mojom::Bitmap::From(*image.ToSkBitmap());
    std::unique_ptr<ChromeShelfItemDelegate> delegate(
        new ChromeShelfItemDelegate(app));
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

void ChromeMashShelfController::OnAlignmentChanged(
    mash::shelf::mojom::Alignment alignment) {
  ash::SetShelfAlignmentPref(
      ProfileManager::GetActiveUserProfile()->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      static_cast<ash::wm::ShelfAlignment>(alignment));
}

void ChromeMashShelfController::OnAutoHideBehaviorChanged(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  ash::SetShelfAutoHideBehaviorPref(
      ProfileManager::GetActiveUserProfile()->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      static_cast<ash::ShelfAutoHideBehavior>(auto_hide));
}

void ChromeMashShelfController::OnAppImageUpdated(const std::string& app_id,
                                                  const gfx::ImageSkia& image) {
  shelf_controller_->SetItemImage(app_id,
                                  skia::mojom::Bitmap::From(*image.bitmap()));
}
