// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_mus.h"

#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/common/common_type_converters.h"
#include "ui/base/resource/resource_bundle.h"

class ChromeShelfItemDelegate : public ash::mojom::ShelfItemDelegate {
 public:
  explicit ChromeShelfItemDelegate(const std::string& app_id,
                                   ChromeLauncherController* controller)
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
  void LaunchItem() override {
    controller_->LaunchApp(app_id_, ash::LAUNCH_FROM_UNKNOWN, ui::EF_NONE);
  }
  void ExecuteCommand(uint32_t command_id, int32_t event_flags) override {
    NOTIMPLEMENTED();
  }
  void ItemPinned() override { NOTIMPLEMENTED(); }
  void ItemUnpinned() override { NOTIMPLEMENTED(); }
  void ItemReordered(uint32_t order) override { NOTIMPLEMENTED(); }

  std::string app_id_;
  mojo::AssociatedBinding<ash::mojom::ShelfItemDelegate> item_delegate_binding_;

  // Not owned.
  ChromeLauncherController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShelfItemDelegate);
};

ChromeLauncherControllerMus::ChromeLauncherControllerMus() {
  AttachProfile(ProfileManager::GetActiveUserProfile());
}

ChromeLauncherControllerMus::~ChromeLauncherControllerMus() {}

void ChromeLauncherControllerMus::Init() {
  NOTIMPLEMENTED();
}

ash::ShelfID ChromeLauncherControllerMus::CreateAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::ShelfItemStatus status) {
  NOTIMPLEMENTED();
  return ash::TYPE_UNDEFINED;
}

void ChromeLauncherControllerMus::SetItemStatus(ash::ShelfID id,
                                                ash::ShelfItemStatus status) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::SetItemController(
    ash::ShelfID id,
    LauncherItemController* controller) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::CloseLauncherItem(ash::ShelfID id) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::Pin(ash::ShelfID id) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::Unpin(ash::ShelfID id) {
  NOTIMPLEMENTED();
}

bool ChromeLauncherControllerMus::IsPinned(ash::ShelfID id) {
  NOTIMPLEMENTED();
  return false;
}

void ChromeLauncherControllerMus::TogglePinned(ash::ShelfID id) {
  NOTIMPLEMENTED();
}

bool ChromeLauncherControllerMus::IsPinnable(ash::ShelfID id) const {
  NOTIMPLEMENTED();
  return false;
}

void ChromeLauncherControllerMus::LockV1AppWithID(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::UnlockV1AppWithID(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::Launch(ash::ShelfID id, int event_flags) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::Close(ash::ShelfID id) {
  NOTIMPLEMENTED();
}

bool ChromeLauncherControllerMus::IsOpen(ash::ShelfID id) {
  NOTIMPLEMENTED();
  return false;
}

bool ChromeLauncherControllerMus::IsPlatformApp(ash::ShelfID id) {
  NOTIMPLEMENTED();
  return false;
}

void ChromeLauncherControllerMus::ActivateApp(const std::string& app_id,
                                              ash::LaunchSource source,
                                              int event_flags) {
  NOTIMPLEMENTED();
}

extensions::LaunchType ChromeLauncherControllerMus::GetLaunchType(
    ash::ShelfID id) {
  NOTIMPLEMENTED();
  return extensions::LAUNCH_TYPE_INVALID;
}

void ChromeLauncherControllerMus::SetLauncherItemImage(
    ash::ShelfID shelf_id,
    const gfx::ImageSkia& image) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::SetLaunchType(
    ash::ShelfID id,
    extensions::LaunchType launch_type) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::UpdateAppState(content::WebContents* contents,
                                                 AppState app_state) {
  NOTIMPLEMENTED();
}

ash::ShelfID ChromeLauncherControllerMus::GetShelfIDForWebContents(
    content::WebContents* contents) {
  NOTIMPLEMENTED();
  return ash::TYPE_UNDEFINED;
}

void ChromeLauncherControllerMus::SetRefocusURLPatternForTest(ash::ShelfID id,
                                                              const GURL& url) {
  NOTIMPLEMENTED();
}

ash::ShelfItemDelegate::PerformedAction
ChromeLauncherControllerMus::ActivateWindowOrMinimizeIfActive(
    ui::BaseWindow* window,
    bool allow_minimize) {
  NOTIMPLEMENTED();
  return ash::ShelfItemDelegate::kNoAction;
}

void ChromeLauncherControllerMus::ActiveUserChanged(
    const std::string& user_email) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::AdditionalUserAddedToSession(
    Profile* profile) {
  NOTIMPLEMENTED();
}

ChromeLauncherAppMenuItems ChromeLauncherControllerMus::GetApplicationList(
    const ash::ShelfItem& item,
    int event_flags) {
  NOTIMPLEMENTED();
  return ChromeLauncherAppMenuItems();
}

std::vector<content::WebContents*>
ChromeLauncherControllerMus::GetV1ApplicationsFromAppId(
    const std::string& app_id) {
  NOTIMPLEMENTED();
  return std::vector<content::WebContents*>();
}

void ChromeLauncherControllerMus::ActivateShellApp(const std::string& app_id,
                                                   int index) {
  NOTIMPLEMENTED();
}

bool ChromeLauncherControllerMus::IsWebContentHandledByApplication(
    content::WebContents* web_contents,
    const std::string& app_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ChromeLauncherControllerMus::ContentCanBeHandledByGmailApp(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return false;
}

gfx::Image ChromeLauncherControllerMus::GetAppListIcon(
    content::WebContents* web_contents) const {
  NOTIMPLEMENTED();
  return gfx::Image();
}

base::string16 ChromeLauncherControllerMus::GetAppListTitle(
    content::WebContents* web_contents) const {
  NOTIMPLEMENTED();
  return base::string16();
}

BrowserShortcutLauncherItemController*
ChromeLauncherControllerMus::GetBrowserShortcutLauncherItemController() {
  NOTIMPLEMENTED();
  return nullptr;
}

LauncherItemController* ChromeLauncherControllerMus::GetLauncherItemController(
    const ash::ShelfID id) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool ChromeLauncherControllerMus::ShelfBoundsChangesProbablyWithUser(
    ash::WmShelf* shelf,
    const AccountId& account_id) const {
  NOTIMPLEMENTED();
  return false;
}

void ChromeLauncherControllerMus::OnUserProfileReadyToSwitch(Profile* profile) {
  NOTIMPLEMENTED();
}

ArcAppDeferredLauncherController*
ChromeLauncherControllerMus::GetArcDeferredLauncher() {
  NOTIMPLEMENTED();
  return nullptr;
}

const std::string& ChromeLauncherControllerMus::GetLaunchIDForShelfID(
    ash::ShelfID id) {
  NOTIMPLEMENTED();
  return base::EmptyString();
}

void ChromeLauncherControllerMus::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image) {
  if (ConnectToShelfController())
    shelf_controller()->SetItemImage(app_id, *image.bitmap());
}

void ChromeLauncherControllerMus::PinAppsFromPrefs() {
  if (!ConnectToShelfController())
    return;

  std::vector<ash::launcher::AppLauncherId> pinned_apps =
      ash::launcher::GetPinnedAppsFromPrefs(profile()->GetPrefs(),
                                            launcher_controller_helper());

  for (const auto& app_launcher_id : pinned_apps) {
    const std::string app_id = app_launcher_id.app_id();
    if (app_launcher_id.ToString() == ash::launcher::kPinnedAppsPlaceholder)
      continue;

    ash::mojom::ShelfItemPtr item(ash::mojom::ShelfItem::New());
    item->app_id = app_id;
    item->app_title = mojo::String::From(
        launcher_controller_helper()->GetAppTitle(profile(), app_id));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Image& image = rb.GetImageNamed(IDR_APP_DEFAULT_ICON);
    item->image = *image.ToSkBitmap();
    std::unique_ptr<ChromeShelfItemDelegate> delegate(
        new ChromeShelfItemDelegate(app_id, this));
    shelf_controller()->PinItem(std::move(item),
                                delegate->CreateInterfacePtrInfoAndBind(
                                    shelf_controller().associated_group()));
    app_id_to_item_delegate_.insert(
        std::make_pair(app_id, std::move(delegate)));

    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
    if (app_icon_loader) {
      app_icon_loader->FetchImage(app_id);
      app_icon_loader->UpdateImage(app_id);
    }
  }
}
