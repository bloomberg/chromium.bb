// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_MUS_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_MUS_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

class ChromeShelfItemDelegate;

class ChromeLauncherControllerMus : public ChromeLauncherController {
 public:
  ChromeLauncherControllerMus();
  ~ChromeLauncherControllerMus() override;

  // ChromeLauncherController:
  void Init() override;
  ash::ShelfID CreateAppLauncherItem(LauncherItemController* controller,
                                     const std::string& app_id,
                                     ash::ShelfItemStatus status) override;
  void SetItemStatus(ash::ShelfID id, ash::ShelfItemStatus status) override;
  void SetItemController(ash::ShelfID id,
                         LauncherItemController* controller) override;
  void CloseLauncherItem(ash::ShelfID id) override;
  void Pin(ash::ShelfID id) override;
  void Unpin(ash::ShelfID id) override;
  bool IsPinned(ash::ShelfID id) override;
  void TogglePinned(ash::ShelfID id) override;
  bool IsPinnable(ash::ShelfID id) const override;
  void LockV1AppWithID(const std::string& app_id) override;
  void UnlockV1AppWithID(const std::string& app_id) override;
  void Launch(ash::ShelfID id, int event_flags) override;
  void Close(ash::ShelfID id) override;
  bool IsOpen(ash::ShelfID id) override;
  bool IsPlatformApp(ash::ShelfID id) override;
  void ActivateApp(const std::string& app_id,
                   ash::LaunchSource source,
                   int event_flags) override;
  extensions::LaunchType GetLaunchType(ash::ShelfID id) override;
  void SetLauncherItemImage(ash::ShelfID shelf_id,
                            const gfx::ImageSkia& image) override;
  void SetLaunchType(ash::ShelfID id,
                     extensions::LaunchType launch_type) override;
  void UpdateAppState(content::WebContents* contents,
                      AppState app_state) override;
  ash::ShelfID GetShelfIDForWebContents(
      content::WebContents* contents) override;
  void SetRefocusURLPatternForTest(ash::ShelfID id, const GURL& url) override;
  ash::ShelfItemDelegate::PerformedAction ActivateWindowOrMinimizeIfActive(
      ui::BaseWindow* window,
      bool allow_minimize) override;
  void ActiveUserChanged(const std::string& user_email) override;
  void AdditionalUserAddedToSession(Profile* profile) override;
  ChromeLauncherAppMenuItems GetApplicationList(const ash::ShelfItem& item,
                                                int event_flags) override;
  std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      const std::string& app_id) override;
  void ActivateShellApp(const std::string& app_id, int index) override;
  bool IsWebContentHandledByApplication(content::WebContents* web_contents,
                                        const std::string& app_id) override;
  bool ContentCanBeHandledByGmailApp(
      content::WebContents* web_contents) override;
  gfx::Image GetAppListIcon(content::WebContents* web_contents) const override;
  base::string16 GetAppListTitle(
      content::WebContents* web_contents) const override;
  BrowserShortcutLauncherItemController*
  GetBrowserShortcutLauncherItemController() override;
  LauncherItemController* GetLauncherItemController(
      const ash::ShelfID id) override;
  bool ShelfBoundsChangesProbablyWithUser(
      ash::WmShelf* shelf,
      const AccountId& account_id) const override;
  void OnUserProfileReadyToSwitch(Profile* profile) override;
  ArcAppDeferredLauncherController* GetArcDeferredLauncher() override;
  const std::string& GetLaunchIDForShelfID(ash::ShelfID id) override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

 private:
  // Pin the items set in the current profile's preferences.
  void PinAppsFromPrefs();

  std::map<std::string, std::unique_ptr<ChromeShelfItemDelegate>>
      app_id_to_item_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerMus);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_MUS_H_
