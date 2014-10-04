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
#include "chrome/grit/locale_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
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
  virtual void Initialize() override {
    UpdateClockType();
  }

  virtual void Shutdown() override {
  }

  virtual bool GetTrayVisibilityOnStartup() override {
    return true;
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const override {
    return ash::user::LOGGED_IN_OWNER;
  }

  virtual void ChangeProfilePicture() override {
  }

  virtual const std::string GetEnterpriseDomain() const override {
    return std::string();
  }

  virtual const base::string16 GetEnterpriseMessage() const override {
    return base::string16();
  }

  virtual const std::string GetSupervisedUserManager() const override {
    return std::string();
  }

  virtual const base::string16 GetSupervisedUserManagerName() const override {
    return base::string16();
  }

  virtual const base::string16 GetSupervisedUserMessage() const override {
    return base::string16();
  }

  virtual bool IsUserSupervised() const override {
    return false;
  }

  virtual bool SystemShouldUpgrade() const override {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual base::HourClockType GetHourClockType() const override {
    return clock_type_;
  }

  virtual void ShowSettings() override {
  }

  virtual bool ShouldShowSettings() override {
    return true;
  }

  virtual void ShowDateSettings() override {
  }

  virtual void ShowSetTimeDialog() override {
  }

  virtual void ShowNetworkSettings(const std::string& service_path) override {
  }

  virtual void ShowBluetoothSettings() override {
  }

  virtual void ShowDisplaySettings() override {
  }

  virtual void ShowChromeSlow() override {
  }

  virtual bool ShouldShowDisplayNotification() override {
    return false;
  }

  virtual void ShowIMESettings() override {
  }

  virtual void ShowHelp() override {
    chrome::ShowHelpForProfile(
        ProfileManager::GetLastUsedProfile(),
        chrome::HOST_DESKTOP_TYPE_ASH,
        chrome::HELP_SOURCE_MENU);
  }

  virtual void ShowAccessibilityHelp() override {
  }

  virtual void ShowAccessibilitySettings() override {
  }

  virtual void ShowPublicAccountInfo() override {
  }

  virtual void ShowSupervisedUserInfo() override {
  }

  virtual void ShowEnterpriseInfo() override {
  }

  virtual void ShowUserLogin() override {
  }

  virtual bool ShowSpringChargerReplacementDialog() override {
    return false;
  }

  virtual bool IsSpringChargerReplacementDialogVisible() override {
    return false;
  }

  virtual bool HasUserConfirmedSafeSpringCharger() override {
    return false;
  }

  virtual void ShutDown() override {
  }

  virtual void SignOut() override {
  }

  virtual void RequestLockScreen() override {
  }

  virtual void RequestRestartForUpdate() override {
    chrome::AttemptRestart();
  }

  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) override {
  }

  virtual void BluetoothStartDiscovering() override {
  }

  virtual void BluetoothStopDiscovering() override {
  }

  virtual void ConnectToBluetoothDevice(const std::string& address) override {
  }

  virtual bool IsBluetoothDiscovering() override {
    return false;
  }

  virtual void GetCurrentIME(ash::IMEInfo* info) override {
  }

  virtual void GetAvailableIMEList(ash::IMEInfoList* list) override {
  }

  virtual void GetCurrentIMEProperties(
      ash::IMEPropertyInfoList* list) override {
  }

  virtual void SwitchIME(const std::string& ime_id) override {
  }

  virtual void ActivateIMEProperty(const std::string& key) override {
  }

  virtual void ShowNetworkConfigure(const std::string& network_id) override {
  }

  virtual bool EnrollNetwork(const std::string& network_id) override {
    return true;
  }

  virtual void ManageBluetoothDevices() override {
  }

  virtual void ToggleBluetooth() override {
  }

  virtual void ShowMobileSimDialog() override {
  }

  virtual void ShowMobileSetupDialog(const std::string& service_path) override {
  }

  virtual void ShowOtherNetworkDialog(const std::string& type) override {
  }

  virtual bool GetBluetoothAvailable() override {
    return false;
  }

  virtual bool GetBluetoothEnabled() override {
    return false;
  }

  virtual bool GetBluetoothDiscovering() override {
    return false;
  }

  virtual void ChangeProxySettings() override {
  }

  virtual ash::VolumeControlDelegate*
  GetVolumeControlDelegate() const override {
    return NULL;
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) override {
  }

  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) override {
    return false;
  }

  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) override {
    return false;
  }

  virtual int GetSystemTrayMenuWidth() override {
    return l10n_util::GetLocalizedContentsWidthInPixels(
        IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
  }

  virtual void ActiveUserWasChanged() override {
  }

  virtual bool IsSearchKeyMappedToCapsLock() override {
    return false;
  }

  virtual ash::tray::UserAccountsDelegate* GetUserAccountsDelegate(
      const std::string& user_id) override {
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
                       const content::NotificationDetails& details) override {
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
