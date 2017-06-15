// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility_types.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace ash {
class SystemTrayNotifier;
}

namespace chromeos {

class SystemTrayDelegateChromeOS
    : public ash::SystemTrayDelegate,
      public content::NotificationObserver,
      public chrome::BrowserListObserver,
      public extensions::AppWindowRegistry::Observer,
      public UpdateEngineClient::Observer {
 public:
  SystemTrayDelegateChromeOS();
  ~SystemTrayDelegateChromeOS() override;

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  void ShowUserLogin() override;
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void SetProfile(Profile* profile);

  bool UnsetProfile(Profile* profile);

  void UpdateShowLogoutButtonInTray();

  void UpdateLogoutDialogDuration();

  void UpdateSessionStartTime();

  void UpdateSessionLengthLimit();

  void StopObservingAppWindowRegistry();

  // Notify observers if the current user has no more open browser or app
  // windows.
  void NotifyIfLastWindowClosed();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnLanguageRemapSearchKeyToChanged();

  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify);

  void UpdatePerformanceTracing();

  // Overridden from chrome::BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // Overridden from extensions::AppWindowRegistry::Observer:
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Overridden from UpdateEngineClient::Observer.
  void OnUpdateOverCellularTargetSet(bool success) override;

  std::unique_ptr<content::NotificationRegistrar> registrar_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_ = nullptr;
  int search_key_mapped_to_ = input_method::kSearchKey;

  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;
  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
