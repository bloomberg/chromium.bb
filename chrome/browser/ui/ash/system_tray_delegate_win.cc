// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_win.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/volume_control_delegate.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/upgrade_detector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"

#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class SystemTrayDelegateWin : public ash::SystemTrayDelegate,
                              public content::NotificationObserver {
 public:
  SystemTrayDelegateWin()
      : clock_type_(base::GetHourClockType()) {
    // Register notifications on construction so that events such as
    // PROFILE_CREATED do not get missed if they happen before Initialize().
    registrar_.reset(new content::NotificationRegistrar);
    registrar_->Add(this,
                    chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                    content::NotificationService::AllSources());
  }

  virtual ~SystemTrayDelegateWin() {
    registrar_.reset();
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual void Initialize() OVERRIDE {
    UpdateClockType();
  }

  virtual void Shutdown() OVERRIDE {
  }

  virtual bool GetTrayVisibilityOnStartup() OVERRIDE {
    return true;
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    return ash::user::LOGGED_IN_OWNER;
  }

  virtual void ChangeProfilePicture() OVERRIDE {
  }

  virtual const std::string GetEnterpriseDomain() const OVERRIDE {
    return std::string();
  }

  virtual const base::string16 GetEnterpriseMessage() const OVERRIDE {
    return base::string16();
  }

  virtual const std::string GetSupervisedUserManager() const OVERRIDE {
    return std::string();
  }

  virtual const base::string16 GetSupervisedUserManagerName() const OVERRIDE {
    return base::string16();
  }

  virtual const base::string16 GetSupervisedUserMessage() const OVERRIDE {
    return base::string16();
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return clock_type_;
  }

  virtual void ShowSettings() OVERRIDE {
  }

  virtual bool ShouldShowSettings() OVERRIDE {
    return true;
  }

  virtual void ShowDateSettings() OVERRIDE {
  }

  virtual void ShowSetTimeDialog() OVERRIDE {
  }

  virtual void ShowNetworkSettings(const std::string& service_path) OVERRIDE {
  }

  virtual void ShowBluetoothSettings() OVERRIDE {
  }

  virtual void ShowDisplaySettings() OVERRIDE {
  }

  virtual void ShowChromeSlow() OVERRIDE {
  }

  virtual bool ShouldShowDisplayNotification() OVERRIDE {
    return false;
  }

  virtual void ShowDriveSettings() OVERRIDE {
  }

  virtual void ShowIMESettings() OVERRIDE {
  }

  virtual void ShowHelp() OVERRIDE {
    chrome::ShowHelpForProfile(
        ProfileManager::GetLastUsedProfile(),
        chrome::HOST_DESKTOP_TYPE_ASH,
        chrome::HELP_SOURCE_MENU);
  }

  virtual void ShowAccessibilityHelp() OVERRIDE {
  }

  virtual void ShowAccessibilitySettings() OVERRIDE {
  }

  virtual void ShowPublicAccountInfo() OVERRIDE {
  }

  virtual void ShowSupervisedUserInfo() OVERRIDE {
  }

  virtual void ShowEnterpriseInfo() OVERRIDE {
  }

  virtual void ShowUserLogin() OVERRIDE {
  }

  virtual bool ShowSpringChargerReplacementDialog() OVERRIDE {
    return false;
  }

  virtual bool IsSpringChargerReplacementDialogVisible() OVERRIDE {
    return false;
  }

  virtual bool HasUserConfirmedSafeSpringCharger() OVERRIDE {
    return false;
  }

  virtual void ShutDown() OVERRIDE {
  }

  virtual void SignOut() OVERRIDE {
  }

  virtual void RequestLockScreen() OVERRIDE {
  }

  virtual void RequestRestartForUpdate() OVERRIDE {
    chrome::AttemptRestart();
  }

  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) OVERRIDE {
  }

  virtual void BluetoothStartDiscovering() OVERRIDE {
  }

  virtual void BluetoothStopDiscovering() OVERRIDE {
  }

  virtual void ConnectToBluetoothDevice(const std::string& address) OVERRIDE {
  }

  virtual bool IsBluetoothDiscovering() OVERRIDE {
    return false;
  }

  virtual void GetCurrentIME(ash::IMEInfo* info) OVERRIDE {
  }

  virtual void GetAvailableIMEList(ash::IMEInfoList* list) OVERRIDE {
  }

  virtual void GetCurrentIMEProperties(
      ash::IMEPropertyInfoList* list) OVERRIDE {
  }

  virtual void SwitchIME(const std::string& ime_id) OVERRIDE {
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
  }

  virtual void CancelDriveOperation(int32 operation_id) OVERRIDE {
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList* list) OVERRIDE {
  }

  virtual void ShowNetworkConfigure(const std::string& network_id,
                                    gfx::NativeWindow parent_window) OVERRIDE {
  }

  virtual bool EnrollNetwork(const std::string& network_id,
                             gfx::NativeWindow parent_window) OVERRIDE {
    return true;
  }

  virtual void ManageBluetoothDevices() OVERRIDE {
  }

  virtual void ToggleBluetooth() OVERRIDE {
  }

  virtual void ShowMobileSimDialog() OVERRIDE {
  }

  virtual void ShowMobileSetupDialog(const std::string& service_path) OVERRIDE {
  }

  virtual void ShowOtherNetworkDialog(const std::string& type) OVERRIDE {
  }

  virtual bool GetBluetoothAvailable() OVERRIDE {
    return false;
  }

  virtual bool GetBluetoothEnabled() OVERRIDE {
    return false;
  }

  virtual bool GetBluetoothDiscovering() OVERRIDE {
    return false;
  }

  virtual void ChangeProxySettings() OVERRIDE {
  }

  virtual ash::VolumeControlDelegate*
  GetVolumeControlDelegate() const OVERRIDE {
    return NULL;
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) OVERRIDE {
  }

  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) OVERRIDE {
    return false;
  }

  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) OVERRIDE {
    return false;
  }

  virtual int GetSystemTrayMenuWidth() OVERRIDE {
    return l10n_util::GetLocalizedContentsWidthInPixels(
        IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
  }

  virtual void ActiveUserWasChanged() OVERRIDE {
  }

  virtual bool IsSearchKeyMappedToCapsLock() OVERRIDE {
    return false;
  }

  virtual ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) OVERRIDE {
    return NULL;
  }

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier() {
    return ash::Shell::GetInstance()->system_tray_notifier();
  }

  void UpdateClockType() {
    clock_type_ = (base::GetHourClockType() == base::k24HourClock) ?
        base::k24HourClock : base::k12HourClock;
    GetSystemTrayNotifier()->NotifyDateFormatChanged();
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_UPGRADE_RECOMMENDED) {
        UpgradeDetector* detector =
            content::Source<UpgradeDetector>(source).ptr();
      ash::UpdateObserver::UpdateSeverity severity =
          ash::UpdateObserver::UPDATE_NORMAL;
      switch (detector->upgrade_notification_stage()) {
        case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
          severity = ash::UpdateObserver::UPDATE_SEVERE_RED;
          break;
        case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
          severity = ash::UpdateObserver::UPDATE_HIGH_ORANGE;
          break;
        case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
          severity = ash::UpdateObserver::UPDATE_LOW_GREEN;
          break;
        case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
        case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
          severity = ash::UpdateObserver::UPDATE_NORMAL;
          break;
      }
      GetSystemTrayNotifier()->NotifyUpdateRecommended(severity);
    } else {
      NOTREACHED();
    }
  }

  scoped_ptr<content::NotificationRegistrar> registrar_;
  base::HourClockType clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateWin);
};

}  // namespace


ash::SystemTrayDelegate* CreateWindowsSystemTrayDelegate() {
  return new SystemTrayDelegateWin();
}
