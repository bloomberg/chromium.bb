// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <vector>

#include "ash/ime/input_method_menu_manager.h"
#include "ash/session/session_state_observer.h"
#include "ash/system/chromeos/supervised/custodian_info_tray_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace user_manager {
class User;
}

namespace chromeos {

class SystemTrayDelegateChromeOS
    : public ash::ime::InputMethodMenuManager::Observer,
      public ash::SystemTrayDelegate,
      public SessionManagerClient::Observer,
      public content::NotificationObserver,
      public input_method::InputMethodManager::Observer,
      public chromeos::LoginState::Observer,
      public chromeos::CrasAudioHandler::AudioObserver,
      public device::BluetoothAdapter::Observer,
      public policy::CloudPolicyStore::Observer,
      public ash::SessionStateObserver,
      public chrome::BrowserListObserver,
      public extensions::AppWindowRegistry::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public SupervisedUserServiceObserver {
 public:
  SystemTrayDelegateChromeOS();

  virtual ~SystemTrayDelegateChromeOS();

  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from ash::SystemTrayDelegate:
  virtual void Initialize() override;
  virtual void Shutdown() override;
  virtual bool GetTrayVisibilityOnStartup() override;
  virtual ash::user::LoginStatus GetUserLoginStatus() const override;
  virtual void ChangeProfilePicture() override;
  virtual const std::string GetEnterpriseDomain() const override;
  virtual const base::string16 GetEnterpriseMessage() const override;
  virtual const std::string GetSupervisedUserManager() const override;
  virtual const base::string16 GetSupervisedUserManagerName() const override;
  virtual const base::string16 GetSupervisedUserMessage() const override;
  virtual bool IsUserSupervised() const override;
  virtual void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
  virtual base::HourClockType GetHourClockType() const override;
  virtual void ShowSettings() override;
  virtual bool ShouldShowSettings() override;
  virtual void ShowDateSettings() override;
  virtual void ShowSetTimeDialog() override;
  virtual void ShowNetworkSettings(const std::string& service_path) override;
  virtual void ShowBluetoothSettings() override;
  virtual void ShowDisplaySettings() override;
  virtual void ShowChromeSlow() override;
  virtual bool ShouldShowDisplayNotification() override;
  virtual void ShowIMESettings() override;
  virtual void ShowHelp() override;
  virtual void ShowAccessibilityHelp() override;
  virtual void ShowAccessibilitySettings() override;
  virtual void ShowPublicAccountInfo() override;
  virtual void ShowSupervisedUserInfo() override;
  virtual void ShowEnterpriseInfo() override;
  virtual void ShowUserLogin() override;
  virtual bool ShowSpringChargerReplacementDialog() override;
  virtual bool IsSpringChargerReplacementDialogVisible() override;
  virtual bool HasUserConfirmedSafeSpringCharger() override;
  virtual void ShutDown() override;
  virtual void SignOut() override;
  virtual void RequestLockScreen() override;
  virtual void RequestRestartForUpdate() override;
  virtual void GetAvailableBluetoothDevices(ash::BluetoothDeviceList* list)
      override;
  virtual void BluetoothStartDiscovering() override;
  virtual void BluetoothStopDiscovering() override;
  virtual void ConnectToBluetoothDevice(const std::string& address) override;
  virtual bool IsBluetoothDiscovering() override;
  virtual void GetCurrentIME(ash::IMEInfo* info) override;
  virtual void GetAvailableIMEList(ash::IMEInfoList* list) override;
  virtual void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) override;
  virtual void SwitchIME(const std::string& ime_id) override;
  virtual void ActivateIMEProperty(const std::string& key) override;
  virtual void ShowNetworkConfigure(const std::string& network_id) override;
  virtual bool EnrollNetwork(const std::string& network_id) override;
  virtual void ManageBluetoothDevices() override;
  virtual void ToggleBluetooth() override;
  virtual void ShowMobileSimDialog() override;
  virtual void ShowMobileSetupDialog(const std::string& service_path) override;
  virtual void ShowOtherNetworkDialog(const std::string& type) override;
  virtual bool GetBluetoothAvailable() override;
  virtual bool GetBluetoothEnabled() override;
  virtual bool GetBluetoothDiscovering() override;
  virtual void ChangeProxySettings() override;
  virtual ash::VolumeControlDelegate* GetVolumeControlDelegate() const override;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) override;
  virtual bool GetSessionStartTime(base::TimeTicks* session_start_time)
      override;
  virtual bool GetSessionLengthLimit(base::TimeDelta* session_length_limit)
      override;
  virtual int GetSystemTrayMenuWidth() override;
  virtual void ActiveUserWasChanged() override;
  virtual bool IsSearchKeyMappedToCapsLock() override;
  virtual ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override;
  virtual void AddCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;
  virtual void RemoveCustodianInfoTrayObserver(
      ash::CustodianInfoTrayObserver* observer) override;

  // Overridden from user_manager::UserManager::UserSessionStateObserver:
  virtual void UserAddedToSession(const user_manager::User* active_user)
      override;

  virtual void UserChangedSupervisedStatus(
      user_manager::User* user) override;

  // browser tests need to call ShouldUse24HourClock().
  bool GetShouldUse24HourClockForTesting() const;

 private:
  // Should be the same as CrosSettings::ObserverSubscription.
  typedef base::CallbackList<void(void)>::Subscription
      CrosSettingsObserverSubscription;

  ash::SystemTray* GetPrimarySystemTray();

  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void SetProfile(Profile* profile);

  bool UnsetProfile(Profile* profile);

  bool ShouldUse24HourClock() const;

  void UpdateClockType();

  void UpdateShowLogoutButtonInTray();

  void UpdateLogoutDialogDuration();

  void UpdateSessionStartTime();

  void UpdateSessionLengthLimit();

  void StopObservingAppWindowRegistry();

  void StopObservingCustodianInfoChanges();

  // Notify observers if the current user has no more open browser or app
  // windows.
  void NotifyIfLastWindowClosed();

  // LoginState::Observer overrides.
  virtual void LoggedInStateChanged() override;

  // Overridden from SessionManagerClient::Observer.
  virtual void ScreenIsLocked() override;
  virtual void ScreenIsUnlocked() override;

  gfx::NativeWindow GetNativeWindow() const;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) override;

  void OnLanguageRemapSearchKeyToChanged();

  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify);

  void UpdatePerformanceTracing();

  // Overridden from InputMethodManager::Observer.
  virtual void InputMethodChanged(input_method::InputMethodManager* manager,
                                  bool show_message) override;

  // Overridden from InputMethodMenuManager::Observer.
  virtual void InputMethodMenuItemChanged(
      ash::ime::InputMethodMenuManager* manager) override;

  // Overridden from CrasAudioHandler::AudioObserver.
  virtual void OnOutputVolumeChanged() override;
  virtual void OnOutputMuteChanged() override;
  virtual void OnInputGainChanged() override;
  virtual void OnInputMuteChanged() override;
  virtual void OnAudioNodesChanged() override;
  virtual void OnActiveOutputNodeChanged() override;
  virtual void OnActiveInputNodeChanged() override;

  // Overridden from BluetoothAdapter::Observer.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) override;
  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool powered) override;
  virtual void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                         bool discovering) override;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) override;
  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) override;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) override;

  void OnStartBluetoothDiscoverySession(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  void UpdateEnterpriseDomain();

  // Overridden from CloudPolicyStore::Observer
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  virtual void OnStoreError(policy::CloudPolicyStore* store) override;

  // Overridden from ash::SessionStateObserver
  virtual void UserAddedToSession(const std::string& user_id) override;

  // Overridden from chrome::BrowserListObserver:
  virtual void OnBrowserRemoved(Browser* browser) override;

  // Overridden from extensions::AppWindowRegistry::Observer:
  virtual void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // Overridden from SupervisedUserServiceObserver:
  virtual void OnCustodianInfoChanged() override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

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
  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;
  scoped_ptr<CrosSettingsObserverSubscription> device_settings_observer_;
  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;
  base::ScopedPtrHashMap<std::string, ash::tray::UserAccountsDelegate>
      accounts_delegates_;

  ObserverList<ash::CustodianInfoTrayObserver>
      custodian_info_changed_observers_;

  base::WeakPtrFactory<SystemTrayDelegateChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
