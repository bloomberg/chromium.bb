// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/metrics/chrome_user_metrics_recorder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace keyboard {
class KeyboardUI;
}

#if defined(OS_CHROMEOS)
namespace chromeos {
class DisplayConfigurationObserver;
}
#endif

class ChromeLauncherController;

class ChromeShellDelegate : public ash::ShellDelegate,
                            public content::NotificationObserver {
 public:
  ChromeShellDelegate();
  ~ChromeShellDelegate() override;

  // ash::ShellDelegate overrides;
  bool IsFirstRunAfterBoot() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsIncognitoAllowed() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void VirtualKeyboardActivated(bool activated) override;
  void AddVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) override;
  void RemoveVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) override;
  void OpenUrl(const GURL& url) override;
  app_list::AppListPresenter* GetAppListPresenter() override;
  ash::ShelfDelegate* CreateShelfDelegate(ash::ShelfModel* model) override;
  ash::SystemTrayDelegate* CreateSystemTrayDelegate() override;
  ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  ash::SessionStateDelegate* CreateSessionStateDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  ash::NewWindowDelegate* CreateNewWindowDelegate() override;
  ash::MediaDelegate* CreateMediaDelegate() override;
  std::unique_ptr<ash::PointerWatcherDelegate> CreatePointerWatcherDelegate()
      override;
  ui::MenuModel* CreateContextMenu(ash::Shelf* shelf,
                                   const ash::ShelfItem* item) override;
  ash::GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  void OpenKeyboardShortcutHelpPage() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  void ToggleTouchpad() override;
  void ToggleTouchscreen() override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void PlatformInit();

  content::NotificationRegistrar registrar_;

  ChromeLauncherController* shelf_delegate_;

  base::ObserverList<ash::VirtualKeyboardStateObserver>
      keyboard_state_observer_list_;

  // Proxies events from chrome/browser to ash::UserMetricsRecorder.
  std::unique_ptr<ChromeUserMetricsRecorder> chrome_user_metrics_recorder_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::DisplayConfigurationObserver>
      display_configuration_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
