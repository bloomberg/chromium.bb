// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/session/session_state_observer.h"
#include "ash/system/chromeos/supervised/custodian_info_tray_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/chromeos/system/system_clock_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/audio/cras_audio_handler.h"
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
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

namespace ash {
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
      public chromeos::CrasAudioHandler::AudioObserver,
      public device::BluetoothAdapter::Observer,
      public policy::CloudPolicyStore::Observer,
      public ash::SessionStateObserver,
      public chrome::BrowserListObserver,
      public extensions::AppWindowRegistry::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public SupervisedUserServiceObserver,
      public ShutdownPolicyHandler::Delegate,
      public system::SystemClockObserver {
 public:
  SystemTrayDelegateChromeOS();

  ~SystemTrayDelegateChromeOS() override;

  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  void Shutdown() override;
  bool GetTrayVisibilityOnStartup() override;
  ash::user::LoginStatus GetUserLoginStatus() const override;
  void ChangeProfilePicture() override;
  std::string GetEnterpriseDomain() const override;
  base::string16 GetEnterpriseMessage() const override;
  std::string GetSupervisedUserManager() const override;
  base::string16 GetSupervisedUserManagerName() const override;
  base::string16 GetSupervisedUserMessage() const override;
  bool IsUserSupervised() const override;
  bool IsUserChild() const override;
  void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
  base::HourClockType GetHourClockType() const override;
  void ShowSettings() override;
  bool ShouldShowSettings() override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowNetworkSettingsForGuid(const std::string& guid) override;
  void ShowDisplaySettings() override;
  void ShowPowerSettings() override;
  void ShowChromeSlow() override;
  bool ShouldShowDisplayNotification() override;
  void ShowIMESettings() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowPublicAccountInfo() override;
  void ShowSupervisedUserInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowUserLogin() override;
  void SignOut() override;
  void RequestLockScreen() override;
  void RequestRestartForUpdate() override;
  void GetAvailableBluetoothDevices(ash::BluetoothDeviceList* list) override;
  void BluetoothStartDiscovering() override;
  void BluetoothStopDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  bool IsBluetoothDiscovering() override;
  void GetCurrentIME(ash::IMEInfo* info) override;
  void GetAvailableIMEList(ash::IMEInfoList* list) override;
  void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  void SwitchIME(const std::string& ime_id) override;
  void ActivateIMEProperty(const std::string& key) override;
  void ManageBluetoothDevices() override;
  void ToggleBluetooth() override;
  void ShowOtherNetworkDialog(const std::string& type) override;
  bool GetBluetoothAvailable() override;
  bool GetBluetoothEnabled() override;
  bool GetBluetoothDiscovering() override;
  void ChangeProxySettings() override;
  ash::CastConfigDelegate* GetCastConfigDelegate() override;
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;
  ash::VolumeControlDelegate* GetVolumeControlDelegate() const override;
  void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  int GetSystemTrayMenuWidth() override;
  void ActiveUserWasChanged() override;
  bool IsSearchKeyMappedToCapsLock() override;
  void AddCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  void RemoveCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  void AddShutdownPolicyObserver(
      ash::ShutdownPolicyObserver* observer) override;
  void RemoveShutdownPolicyObserver(
      ash::ShutdownPolicyObserver* observer) override;
  void ShouldRebootOnShutdown(
      const ash::RebootOnShutdownCallback& callback) override;
  ash::VPNDelegate* GetVPNDelegate() const override;

  // Overridden from user_manager::UserManager::UserSessionStateObserver:
  void UserAddedToSession(const user_manager::User* active_user) override;
  void ActiveUserChanged(const user_manager::User* active_user) override;

  void UserChangedChildStatus(user_manager::User* user) override;

  // browser tests need to call ShouldUse24HourClock().
  bool GetShouldUse24HourClockForTesting() const;

  // chromeos::system::SystemClockObserver implementation.
  void OnSystemClockChanged(system::SystemClock*) override;

 private:

  ash::SystemTray* GetPrimarySystemTray();

  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void SetProfile(Profile* profile);

  bool UnsetProfile(Profile* profile);

  bool ShouldUse24HourClock() const;

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

  gfx::NativeWindow GetNativeWindow() const;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnLanguageRemapSearchKeyToChanged();

  void OnAccessibilityModeChanged(
      ui::AccessibilityNotificationVisibility notify);

  void UpdatePerformanceTracing();

  // Overridden from InputMethodManager::Observer.
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Overridden from InputMethodMenuManager::Observer.
  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override;

  // Overridden from CrasAudioHandler::AudioObserver.
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on, bool system_adjust) override;
  void OnInputNodeGainChanged(uint64_t node_id, int gain) override;
  void OnInputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

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
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

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

  // Overridden from ShutdownPolicyObserver::Delegate.
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // helper methods used by GetSupervisedUserMessage.
  const base::string16 GetLegacySupervisedUserMessage() const;
  const base::string16 GetChildUserMessage() const;

  scoped_ptr<content::NotificationRegistrar> registrar_;
  scoped_ptr<PrefChangeRegistrar> local_state_registrar_;
  scoped_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_;
  base::HourClockType clock_type_;
  int search_key_mapped_to_;
  bool screen_locked_;
  bool have_session_start_time_;
  base::TimeTicks session_start_time_;
  bool have_session_length_limit_;
  base::TimeDelta session_length_limit_;
  std::string enterprise_domain_;
  bool should_run_bluetooth_discovery_;
  bool session_started_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  scoped_ptr<device::BluetoothDiscoverySession> bluetooth_discovery_session_;
  scoped_ptr<ash::CastConfigDelegate> cast_config_delegate_;
  scoped_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;
  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;
  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;
  scoped_ptr<ShutdownPolicyHandler> shutdown_policy_handler_;
  scoped_ptr<ash::VPNDelegate> vpn_delegate_;

  base::ObserverList<ash::CustodianInfoTrayObserver>
      custodian_info_changed_observers_;

  base::ObserverList<ash::ShutdownPolicyObserver> shutdown_policy_observers_;

  base::WeakPtrFactory<SystemTrayDelegateChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
