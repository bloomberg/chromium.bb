// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_mus.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_mash_shelf_controller.h"

// static
ChromeLauncherController* ChromeLauncherControllerMus::CreateInstance() {
  DCHECK(!ChromeLauncherController::instance());
  ChromeLauncherControllerMus* instance = new ChromeLauncherControllerMus();
  ChromeLauncherController::set_instance(instance);
  return instance;
}

ChromeLauncherControllerMus::ChromeLauncherControllerMus()
    : shelf_controller_(new ChromeMashShelfController) {}

ChromeLauncherControllerMus::~ChromeLauncherControllerMus() {}

void ChromeLauncherControllerMus::Init() {}

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

void ChromeLauncherControllerMus::LaunchApp(const std::string& app_id,
                                            ash::LaunchSource source,
                                            int event_flags) {
  shelf_controller_->LaunchItem(app_id);
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

bool ChromeLauncherControllerMus::IsWindowedAppInLauncher(
    const std::string& app_id) {
  NOTIMPLEMENTED();
  return false;
}

void ChromeLauncherControllerMus::SetLaunchType(
    ash::ShelfID id,
    extensions::LaunchType launch_type) {
  NOTIMPLEMENTED();
}

Profile* ChromeLauncherControllerMus::GetProfile() {
  return ProfileManager::GetActiveUserProfile();
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
    ash::Shelf* shelf,
    const std::string& user_id) const {
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
