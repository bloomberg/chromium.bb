// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_mus.h"

#include "ash/public/cpp/app_launch_id.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_constants.h"

ChromeLauncherControllerMus::ChromeLauncherControllerMus() {
  AttachProfile(ProfileManager::GetActiveUserProfile());
}

ChromeLauncherControllerMus::~ChromeLauncherControllerMus() {}

ash::ShelfID ChromeLauncherControllerMus::CreateAppLauncherItem(
    LauncherItemController* controller,
    ash::ShelfItemStatus status) {
  NOTIMPLEMENTED();
  return ash::TYPE_UNDEFINED;
}

const ash::ShelfItem* ChromeLauncherControllerMus::GetItem(
    ash::ShelfID id) const {
  NOTIMPLEMENTED();
  return nullptr;
}

void ChromeLauncherControllerMus::SetItemType(ash::ShelfID id,
                                              ash::ShelfItemType type) {
  NOTIMPLEMENTED();
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
                                              ash::ShelfLaunchSource source,
                                              int event_flags) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::SetLauncherItemImage(
    ash::ShelfID shelf_id,
    const gfx::ImageSkia& image) {
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

ash::ShelfAction ChromeLauncherControllerMus::ActivateWindowOrMinimizeIfActive(
    ui::BaseWindow* window,
    bool allow_minimize) {
  NOTIMPLEMENTED();
  return ash::SHELF_ACTION_NONE;
}

void ChromeLauncherControllerMus::ActiveUserChanged(
    const std::string& user_email) {
  NOTIMPLEMENTED();
}

void ChromeLauncherControllerMus::AdditionalUserAddedToSession(
    Profile* profile) {
  NOTIMPLEMENTED();
}

MenuItemList ChromeLauncherControllerMus::GetAppMenuItemsForTesting(
    const ash::ShelfItem& item) {
  NOTIMPLEMENTED();
  return MenuItemList();
}

std::vector<content::WebContents*>
ChromeLauncherControllerMus::GetV1ApplicationsFromAppId(
    const std::string& app_id) {
  NOTIMPLEMENTED();
  return std::vector<content::WebContents*>();
}

void ChromeLauncherControllerMus::ActivateShellApp(const std::string& app_id,
                                                   int window_index) {
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

void ChromeLauncherControllerMus::OnInit() {}

void ChromeLauncherControllerMus::PinAppsFromPrefs() {
  if (!ConnectToShelfController())
    return;

  std::vector<ash::AppLaunchId> pinned_apps =
      ash::launcher::GetPinnedAppsFromPrefs(profile()->GetPrefs(),
                                            launcher_controller_helper());

  for (const auto& app_launch_id : pinned_apps) {
    const std::string app_id = app_launch_id.app_id();
    if (app_launch_id.app_id() == ash::launcher::kPinnedAppsPlaceholder)
      continue;

    ash::mojom::ShelfItemPtr item(ash::mojom::ShelfItem::New());
    item->app_id = app_id;
    item->title = launcher_controller_helper()->GetAppTitle(profile(), app_id);
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Image& image = rb.GetImageNamed(IDR_APP_DEFAULT_ICON);
    item->image = *image.ToSkBitmap();
    // TOOD(msw): Actually pin the item and install its delegate; this code is
    // unused at the moment. See http://crbug.com/647879
    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
    if (app_icon_loader) {
      app_icon_loader->FetchImage(app_id);
      app_icon_loader->UpdateImage(app_id);
    }
  }
}
