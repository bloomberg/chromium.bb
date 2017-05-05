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
#include "ash/system/tray/ime_info.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

namespace ash {
class SystemTrayNotifier;
}

namespace chromeos {

class SystemTrayDelegateChromeOS
    : public ui::ime::InputMethodMenuManager::Observer,
      public ash::SystemTrayDelegate,
      public content::NotificationObserver,
      public input_method::InputMethodManager::Observer,
      public chrome::BrowserListObserver,
      public extensions::AppWindowRegistry::Observer,
      public input_method::InputMethodManager::ImeMenuObserver {
 public:
  SystemTrayDelegateChromeOS();
  ~SystemTrayDelegateChromeOS() override;

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  void ShowUserLogin() override;
  void GetCurrentIME(ash::IMEInfo* info) override;
  void GetAvailableIMEList(ash::IMEInfoList* list) override;
  void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  base::string16 GetIMEManagedMessage() override;
  void SwitchIME(const std::string& ime_id) override;
  void ActivateIMEProperty(const std::string& key) override;
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;
  std::unique_ptr<ash::SystemTrayItem> CreateRotationLockTrayItem(
      ash::SystemTray* tray) override;

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

  // Overridden from InputMethodManager::Observer.
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Overridden from InputMethodMenuManager::Observer.
  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override;

  // Overridden from chrome::BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // Overridden from extensions::AppWindowRegistry::Observer:
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool is_active) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<input_method::InputMethodManager::MenuItem>& items)
      override;

  std::unique_ptr<content::NotificationRegistrar> registrar_;
  std::unique_ptr<PrefChangeRegistrar> local_state_registrar_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_ = nullptr;
  int search_key_mapped_to_ = input_method::kSearchKey;
  bool have_session_start_time_ = false;
  base::TimeTicks session_start_time_;
  bool have_session_length_limit_ = false;
  base::TimeDelta session_length_limit_;
  bool session_started_ = false;

  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;
  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
