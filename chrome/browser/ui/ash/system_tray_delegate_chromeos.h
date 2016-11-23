// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/common/accessibility_types.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/common/system/chromeos/supervised/custodian_info_tray_observer.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

namespace ash {
class SystemTrayNotifier;
class VPNDelegate;
}

namespace user_manager {
class User;
}

namespace chromeos {

class SystemTrayDelegateChromeOS
    : public ui::ime::InputMethodMenuManager::Observer,
      public ash::SystemTrayDelegate,
      public SessionManagerClient::Observer,
      public content::NotificationObserver,
      public input_method::InputMethodManager::Observer,
      public device::BluetoothAdapter::Observer,
      public policy::CloudPolicyStore::Observer,
      public ash::SessionStateObserver,
      public chrome::BrowserListObserver,
      public extensions::AppWindowRegistry::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public SupervisedUserServiceObserver,
      public input_method::InputMethodManager::ImeMenuObserver {
 public:
  SystemTrayDelegateChromeOS();

  ~SystemTrayDelegateChromeOS() override;

  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  ash::LoginStatus GetUserLoginStatus() const override;
  std::string GetEnterpriseDomain() const override;
  std::string GetEnterpriseRealm() const override;
  base::string16 GetEnterpriseMessage() const override;
  std::string GetSupervisedUserManager() const override;
  base::string16 GetSupervisedUserManagerName() const override;
  base::string16 GetSupervisedUserMessage() const override;
  bool IsUserSupervised() const override;
  bool IsUserChild() const override;
  void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
  bool ShouldShowSettings() const override;
  bool ShouldShowNotificationTray() const override;
  void ShowEnterpriseInfo() override;
  void ShowUserLogin() override;
  void GetAvailableBluetoothDevices(ash::BluetoothDeviceList* list) override;
  void BluetoothStartDiscovering() override;
  void BluetoothStopDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  bool IsBluetoothDiscovering() const override;
  void GetCurrentIME(ash::IMEInfo* info) override;
  void GetAvailableIMEList(ash::IMEInfoList* list) override;
  void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  void SwitchIME(const std::string& ime_id) override;
  void ActivateIMEProperty(const std::string& key) override;
  void ManageBluetoothDevices() override;
  void ToggleBluetooth() override;
  bool GetBluetoothAvailable() override;
  bool GetBluetoothEnabled() override;
  bool GetBluetoothDiscovering() override;
  ash::CastConfigDelegate* GetCastConfigDelegate() override;
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  int GetSystemTrayMenuWidth() override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;
  void AddCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  void RemoveCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  ash::VPNDelegate* GetVPNDelegate() const override;
  std::unique_ptr<ash::SystemTrayItem> CreateRotationLockTrayItem(
      ash::SystemTray* tray) override;

  // Overridden from user_manager::UserManager::UserSessionStateObserver:
  void UserAddedToSession(const user_manager::User* active_user) override;
  void ActiveUserChanged(const user_manager::User* active_user) override;

  void UserChangedChildStatus(user_manager::User* user) override;

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void SetProfile(Profile* profile);

  bool UnsetProfile(Profile* profile);

  void UpdateShowLogoutButtonInTray();

  void UpdateLogoutDialogDuration();

  void UpdateSessionStartTime();

  void UpdateSessionLengthLimit();

  void StopObservingAppWindowRegistry();

  void StopObservingCustodianInfoChanges();

  // Notify observers if the current user has no more open browser or app
  // windows.
  void NotifyIfLastWindowClosed();

  // Overridden from SessionManagerClient::Observer.
  void ScreenIsLocked() override;
  void ScreenIsUnlocked() override;

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

  // Overridden from BluetoothAdapter::Observer.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void OnStartBluetoothDiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  void UpdateEnterpriseDomain();

  // Overridden from CloudPolicyStore::Observer
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // Overridden from ash::SessionStateObserver
  void UserAddedToSession(const AccountId& account_id) override;
  void ActiveUserChanged(const AccountId& account_id) override;

  // Overridden from chrome::BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // Overridden from extensions::AppWindowRegistry::Observer:
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // Overridden from SupervisedUserServiceObserver:
  void OnCustodianInfoChanged() override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool is_active) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<input_method::InputMethodManager::MenuItem>& items)
      override;

  // helper methods used by GetSupervisedUserMessage.
  const base::string16 GetLegacySupervisedUserMessage() const;
  const base::string16 GetChildUserMessage() const;

  std::unique_ptr<content::NotificationRegistrar> registrar_;
  std::unique_ptr<PrefChangeRegistrar> local_state_registrar_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_ = nullptr;
  int search_key_mapped_to_ = input_method::kSearchKey;
  bool have_session_start_time_ = false;
  base::TimeTicks session_start_time_;
  bool have_session_length_limit_ = false;
  base::TimeDelta session_length_limit_;
  std::string enterprise_domain_;
  std::string enterprise_realm_;
  bool should_run_bluetooth_discovery_ = false;
  bool session_started_ = false;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<device::BluetoothDiscoverySession>
      bluetooth_discovery_session_;
  std::unique_ptr<ash::CastConfigDelegate> cast_config_delegate_;
  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;
  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;
  std::unique_ptr<ash::VPNDelegate> vpn_delegate_;

  base::ObserverList<ash::CustodianInfoTrayObserver>
      custodian_info_changed_observers_;

  base::WeakPtrFactory<SystemTrayDelegateChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
