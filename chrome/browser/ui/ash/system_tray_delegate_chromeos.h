// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include "apps/app_window_registry.h"
#include "ash/ime/input_method_menu_manager.h"
#include "ash/session/session_state_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/job_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace chromeos {

class SystemTrayDelegateChromeOS
    : public ash::ime::InputMethodMenuManager::Observer,
      public ash::SystemTrayDelegate,
      public SessionManagerClient::Observer,
      public drive::JobListObserver,
      public content::NotificationObserver,
      public input_method::InputMethodManager::Observer,
      public chromeos::LoginState::Observer,
      public chromeos::CrasAudioHandler::AudioObserver,
      public device::BluetoothAdapter::Observer,
      public policy::CloudPolicyStore::Observer,
      public ash::SessionStateObserver,
      public chrome::BrowserListObserver,
      public apps::AppWindowRegistry::Observer {
 public:
  SystemTrayDelegateChromeOS();

  virtual ~SystemTrayDelegateChromeOS();

  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from ash::SystemTrayDelegate:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE;
  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE;
  virtual void ChangeProfilePicture() OVERRIDE;
  virtual const std::string GetEnterpriseDomain() const OVERRIDE;
  virtual const base::string16 GetEnterpriseMessage() const OVERRIDE;
  virtual const std::string GetSupervisedUserManager() const OVERRIDE;
  virtual const base::string16 GetSupervisedUserManagerName() const OVERRIDE;
  virtual const base::string16 GetSupervisedUserMessage() const OVERRIDE;
  virtual bool SystemShouldUpgrade() const OVERRIDE;
  virtual base::HourClockType GetHourClockType() const OVERRIDE;
  virtual void ShowSettings() OVERRIDE;
  virtual bool ShouldShowSettings() OVERRIDE;
  virtual void ShowDateSettings() OVERRIDE;
  virtual void ShowSetTimeDialog() OVERRIDE;
  virtual void ShowNetworkSettings(const std::string& service_path) OVERRIDE;
  virtual void ShowBluetoothSettings() OVERRIDE;
  virtual void ShowDisplaySettings() OVERRIDE;
  virtual void ShowChromeSlow() OVERRIDE;
  virtual bool ShouldShowDisplayNotification() OVERRIDE;
  virtual void ShowDriveSettings() OVERRIDE;
  virtual void ShowIMESettings() OVERRIDE;
  virtual void ShowHelp() OVERRIDE;
  virtual void ShowAccessibilityHelp() OVERRIDE;
  virtual void ShowAccessibilitySettings() OVERRIDE;
  virtual void ShowPublicAccountInfo() OVERRIDE;
  virtual void ShowSupervisedUserInfo() OVERRIDE;
  virtual void ShowEnterpriseInfo() OVERRIDE;
  virtual void ShowUserLogin() OVERRIDE;
  virtual bool ShowSpringChargerReplacementDialog() OVERRIDE;
  virtual bool IsSpringChargerReplacementDialogVisible() OVERRIDE;
  virtual bool HasUserConfirmedSafeSpringCharger() OVERRIDE;
  virtual void ShutDown() OVERRIDE;
  virtual void SignOut() OVERRIDE;
  virtual void RequestLockScreen() OVERRIDE;
  virtual void RequestRestartForUpdate() OVERRIDE;
  virtual void GetAvailableBluetoothDevices(ash::BluetoothDeviceList* list)
      OVERRIDE;
  virtual void BluetoothStartDiscovering() OVERRIDE;
  virtual void BluetoothStopDiscovering() OVERRIDE;
  virtual void ConnectToBluetoothDevice(const std::string& address) OVERRIDE;
  virtual bool IsBluetoothDiscovering() OVERRIDE;
  virtual void GetCurrentIME(ash::IMEInfo* info) OVERRIDE;
  virtual void GetAvailableIMEList(ash::IMEInfoList* list) OVERRIDE;
  virtual void GetCurrentIMEProperties(ash::IMEPropertyInfoList* list) OVERRIDE;
  virtual void SwitchIME(const std::string& ime_id) OVERRIDE;
  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE;
  virtual void CancelDriveOperation(int32 operation_id) OVERRIDE;
  virtual void GetDriveOperationStatusList(ash::DriveOperationStatusList* list)
      OVERRIDE;
  virtual void ShowNetworkConfigure(const std::string& network_id,
                                    gfx::NativeWindow parent_window) OVERRIDE;
  virtual bool EnrollNetwork(const std::string& network_id,
                             gfx::NativeWindow parent_window) OVERRIDE;
  virtual void ManageBluetoothDevices() OVERRIDE;
  virtual void ToggleBluetooth() OVERRIDE;
  virtual void ShowMobileSimDialog() OVERRIDE;
  virtual void ShowMobileSetupDialog(const std::string& service_path) OVERRIDE;
  virtual void ShowOtherNetworkDialog(const std::string& type) OVERRIDE;
  virtual bool GetBluetoothAvailable() OVERRIDE;
  virtual bool GetBluetoothEnabled() OVERRIDE;
  virtual bool GetBluetoothDiscovering() OVERRIDE;
  virtual void ChangeProxySettings() OVERRIDE;
  virtual ash::VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE;
  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) OVERRIDE;
  virtual bool GetSessionStartTime(base::TimeTicks* session_start_time)
      OVERRIDE;
  virtual bool GetSessionLengthLimit(base::TimeDelta* session_length_limit)
      OVERRIDE;
  virtual int GetSystemTrayMenuWidth() OVERRIDE;
  virtual void ActiveUserWasChanged() OVERRIDE;
  virtual bool IsSearchKeyMappedToCapsLock() OVERRIDE;
  virtual ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) OVERRIDE;

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

  void ObserveDriveUpdates();

  void UnobserveDriveUpdates();

  bool ShouldUse24HourClock() const;

  void UpdateClockType();

  void UpdateShowLogoutButtonInTray();

  void UpdateLogoutDialogDuration();

  void UpdateSessionStartTime();

  void UpdateSessionLengthLimit();

  void StopObservingAppWindowRegistry();

  // Notify observers if the current user has no more open browser or app
  // windows.
  void NotifyIfLastWindowClosed();

  // LoginState::Observer overrides.
  virtual void LoggedInStateChanged() OVERRIDE;

  // Overridden from SessionManagerClient::Observer.
  virtual void ScreenIsLocked() OVERRIDE;
  virtual void ScreenIsUnlocked() OVERRIDE;

  gfx::NativeWindow GetNativeWindow() const;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnLanguageRemapSearchKeyToChanged();

  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify);

  void UpdatePerformanceTracing();

  // Overridden from InputMethodManager::Observer.
  virtual void InputMethodChanged(input_method::InputMethodManager* manager,
                                  bool show_message) OVERRIDE;

  // Overridden from InputMethodMenuManager::Observer.
  virtual void InputMethodMenuItemChanged(
      ash::ime::InputMethodMenuManager* manager) OVERRIDE;

  // Overridden from CrasAudioHandler::AudioObserver.
  virtual void OnOutputVolumeChanged() OVERRIDE;
  virtual void OnOutputMuteChanged() OVERRIDE;
  virtual void OnInputGainChanged() OVERRIDE;
  virtual void OnInputMuteChanged() OVERRIDE;
  virtual void OnAudioNodesChanged() OVERRIDE;
  virtual void OnActiveOutputNodeChanged() OVERRIDE;
  virtual void OnActiveInputNodeChanged() OVERRIDE;

  // drive::JobListObserver overrides.
  virtual void OnJobAdded(const drive::JobInfo& job_info) OVERRIDE;

  virtual void OnJobDone(const drive::JobInfo& job_info,
                         drive::FileError error) OVERRIDE;

  virtual void OnJobUpdated(const drive::JobInfo& job_info) OVERRIDE;

  drive::DriveIntegrationService* FindDriveIntegrationService();

  // Overridden from BluetoothAdapter::Observer.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE;
  virtual void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

  void OnStartBluetoothDiscoverySession(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  void UpdateEnterpriseDomain();

  // Overridden from CloudPolicyStore::Observer
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE;

  // Overridden from ash::SessionStateObserver
  virtual void UserAddedToSession(const std::string& user_id) OVERRIDE;

  // Overridden from chrome::BrowserListObserver:
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // Overridden from apps::AppWindowRegistry::Observer:
  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  base::WeakPtrFactory<SystemTrayDelegateChromeOS> weak_ptr_factory_;
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

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  scoped_ptr<device::BluetoothDiscoverySession> bluetooth_discovery_session_;
  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;
  scoped_ptr<CrosSettingsObserverSubscription> device_settings_observer_;
  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;
  base::ScopedPtrHashMap<std::string, ash::tray::UserAccountsDelegate>
      accounts_delegates_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
