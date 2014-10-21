// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <string>

#include "ash/shelf/shelf_item_types.h"
#include "ash/shell_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;

namespace ash {
class ShelfItemDelegate;
}

namespace content {
class WebContents;
}

namespace keyboard {
class KeyboardControllerProxy;
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

  static ChromeShellDelegate* instance() { return instance_; }

  // ash::ShellDelegate overrides;
  bool IsFirstRunAfterBoot() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsIncognitoAllowed() const override;
  bool IsRunningInForcedAppMode() const override;
  bool IsMultiAccountEnabled() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardControllerProxy* CreateKeyboardControllerProxy() override;
  void VirtualKeyboardActivated(bool activated) override;
  void AddVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) override;
  void RemoveVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) override;
  content::BrowserContext* GetActiveBrowserContext() override;
  app_list::AppListViewDelegate* GetAppListViewDelegate() override;
  ash::ShelfDelegate* CreateShelfDelegate(ash::ShelfModel* model) override;
  ash::SystemTrayDelegate* CreateSystemTrayDelegate() override;
  ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  ash::SessionStateDelegate* CreateSessionStateDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  ash::NewWindowDelegate* CreateNewWindowDelegate() override;
  ash::MediaDelegate* CreateMediaDelegate() override;
  ui::MenuModel* CreateContextMenu(aura::Window* root,
                                   ash::ShelfItemDelegate* item_delegate,
                                   ash::ShelfItem* item) override;
  ash::GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void PlatformInit();

  static ChromeShellDelegate* instance_;

  content::NotificationRegistrar registrar_;

  ChromeLauncherController* shelf_delegate_;

  ObserverList<ash::VirtualKeyboardStateObserver> keyboard_state_observer_list_;

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::DisplayConfigurationObserver>
      display_configuration_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
