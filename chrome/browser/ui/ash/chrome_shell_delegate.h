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
  virtual ~ChromeShellDelegate();

  static ChromeShellDelegate* instance() { return instance_; }

  // ash::ShellDelegate overrides;
  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsMultiProfilesEnabled() const OVERRIDE;
  virtual bool IsIncognitoAllowed() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual bool IsMultiAccountEnabled() const OVERRIDE;
  virtual void PreInit() OVERRIDE;
  virtual void PreShutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() OVERRIDE;
  virtual void VirtualKeyboardActivated(bool activated) OVERRIDE;
  virtual void AddVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual void RemoveVirtualKeyboardStateObserver(
      ash::VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual content::BrowserContext* GetActiveBrowserContext() OVERRIDE;
  virtual app_list::AppListViewDelegate* GetAppListViewDelegate() OVERRIDE;
  virtual ash::ShelfDelegate* CreateShelfDelegate(
      ash::ShelfModel* model) OVERRIDE;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual ash::SessionStateDelegate* CreateSessionStateDelegate() OVERRIDE;
  virtual ash::AccessibilityDelegate* CreateAccessibilityDelegate() OVERRIDE;
  virtual ash::NewWindowDelegate* CreateNewWindowDelegate() OVERRIDE;
  virtual ash::MediaDelegate* CreateMediaDelegate() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root,
      ash::ShelfItemDelegate* item_delegate,
      ash::ShelfItem* item) OVERRIDE;
  virtual ash::GPUSupport* CreateGPUSupport() OVERRIDE;
  virtual base::string16 GetProductName() const OVERRIDE;

  // content::NotificationObserver override:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
