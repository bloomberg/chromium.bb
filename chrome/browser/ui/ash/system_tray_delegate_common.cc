// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_common.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/volume_control_delegate.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_tray_delegate_utils.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/grit/locale_settings.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util.h"

SystemTrayDelegateCommon::SystemTrayDelegateCommon()
    : clock_type_(base::GetHourClockType()) {
  // Register notifications on construction so that events such as
  // PROFILE_CREATED do not get missed if they happen before Initialize().
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this,
                  chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                  content::NotificationService::AllSources());
}

SystemTrayDelegateCommon::~SystemTrayDelegateCommon() {
  registrar_.reset();
}

// Overridden from ash::SystemTrayDelegate:
void SystemTrayDelegateCommon::Initialize() {
  UpdateClockType();
}

void SystemTrayDelegateCommon::Shutdown() {
}

bool SystemTrayDelegateCommon::GetTrayVisibilityOnStartup() {
  return true;
}

ash::user::LoginStatus SystemTrayDelegateCommon::GetUserLoginStatus() const {
  return ash::user::LOGGED_IN_OWNER;
}

void SystemTrayDelegateCommon::ChangeProfilePicture() {
}

const std::string SystemTrayDelegateCommon::GetEnterpriseDomain() const {
  return std::string();
}

const base::string16 SystemTrayDelegateCommon::GetEnterpriseMessage() const {
  return base::string16();
}

const std::string SystemTrayDelegateCommon::GetSupervisedUserManager() const {
  return std::string();
}

const base::string16 SystemTrayDelegateCommon::GetSupervisedUserManagerName()
    const {
  return base::string16();
}

const base::string16 SystemTrayDelegateCommon::GetSupervisedUserMessage()
    const {
  return base::string16();
}

bool SystemTrayDelegateCommon::IsUserSupervised() const {
  return false;
}

bool SystemTrayDelegateCommon::IsUserChild() const {
  return false;
}

void SystemTrayDelegateCommon::GetSystemUpdateInfo(
    ash::UpdateInfo* info) const {
  GetUpdateInfo(UpgradeDetector::GetInstance(), info);
}

base::HourClockType SystemTrayDelegateCommon::GetHourClockType() const {
  return clock_type_;
}

void SystemTrayDelegateCommon::ShowSettings() {
}

bool SystemTrayDelegateCommon::ShouldShowSettings() {
  return true;
}

void SystemTrayDelegateCommon::ShowDateSettings() {
}

void SystemTrayDelegateCommon::ShowSetTimeDialog() {
}

void SystemTrayDelegateCommon::ShowNetworkSettings(
    const std::string& service_path) {
}

void SystemTrayDelegateCommon::ShowBluetoothSettings() {
}

void SystemTrayDelegateCommon::ShowDisplaySettings() {
}

void SystemTrayDelegateCommon::ShowChromeSlow() {
#if defined(OS_LINUX)
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSlow(displayer.browser());
#endif  // defined(OS_LINUX)
}

bool SystemTrayDelegateCommon::ShouldShowDisplayNotification() {
  return false;
}

void SystemTrayDelegateCommon::ShowIMESettings() {
}

void SystemTrayDelegateCommon::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetLastUsedProfile(),
                             chrome::HOST_DESKTOP_TYPE_ASH,
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayDelegateCommon::ShowAccessibilityHelp() {
}

void SystemTrayDelegateCommon::ShowAccessibilitySettings() {
}

void SystemTrayDelegateCommon::ShowPublicAccountInfo() {
}

void SystemTrayDelegateCommon::ShowSupervisedUserInfo() {
}

void SystemTrayDelegateCommon::ShowEnterpriseInfo() {
}

void SystemTrayDelegateCommon::ShowUserLogin() {
}

void SystemTrayDelegateCommon::ShutDown() {
}

void SystemTrayDelegateCommon::SignOut() {
}

void SystemTrayDelegateCommon::RequestLockScreen() {
}

void SystemTrayDelegateCommon::RequestRestartForUpdate() {
  chrome::AttemptRestart();
}

void SystemTrayDelegateCommon::GetAvailableBluetoothDevices(
    ash::BluetoothDeviceList* list) {
}

void SystemTrayDelegateCommon::BluetoothStartDiscovering() {
}

void SystemTrayDelegateCommon::BluetoothStopDiscovering() {
}

void SystemTrayDelegateCommon::ConnectToBluetoothDevice(
    const std::string& address) {
}

bool SystemTrayDelegateCommon::IsBluetoothDiscovering() {
  return false;
}

void SystemTrayDelegateCommon::GetCurrentIME(ash::IMEInfo* info) {
}

void SystemTrayDelegateCommon::GetAvailableIMEList(ash::IMEInfoList* list) {
}

void SystemTrayDelegateCommon::GetCurrentIMEProperties(
    ash::IMEPropertyInfoList* list) {
}

void SystemTrayDelegateCommon::SwitchIME(const std::string& ime_id) {
}

void SystemTrayDelegateCommon::ActivateIMEProperty(const std::string& key) {
}

void SystemTrayDelegateCommon::ManageBluetoothDevices() {
}

void SystemTrayDelegateCommon::ToggleBluetooth() {
}

void SystemTrayDelegateCommon::ShowOtherNetworkDialog(const std::string& type) {
}

bool SystemTrayDelegateCommon::GetBluetoothAvailable() {
  return false;
}

bool SystemTrayDelegateCommon::GetBluetoothEnabled() {
  return false;
}

bool SystemTrayDelegateCommon::GetBluetoothDiscovering() {
  return false;
}

void SystemTrayDelegateCommon::ChangeProxySettings() {
}

ash::VolumeControlDelegate* SystemTrayDelegateCommon::GetVolumeControlDelegate()
    const {
  return NULL;
}

void SystemTrayDelegateCommon::SetVolumeControlDelegate(
    scoped_ptr<ash::VolumeControlDelegate> delegate) {
}

bool SystemTrayDelegateCommon::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool SystemTrayDelegateCommon::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  return false;
}

int SystemTrayDelegateCommon::GetSystemTrayMenuWidth() {
  return l10n_util::GetLocalizedContentsWidthInPixels(
      IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
}

void SystemTrayDelegateCommon::ActiveUserWasChanged() {
}

bool SystemTrayDelegateCommon::IsSearchKeyMappedToCapsLock() {
  return false;
}

ash::tray::UserAccountsDelegate*
SystemTrayDelegateCommon::GetUserAccountsDelegate(const std::string& user_id) {
  return NULL;
}

void SystemTrayDelegateCommon::AddCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
}

void SystemTrayDelegateCommon::RemoveCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
}

ash::SystemTrayNotifier* SystemTrayDelegateCommon::GetSystemTrayNotifier() {
  return ash::Shell::GetInstance()->system_tray_notifier();
}

void SystemTrayDelegateCommon::UpdateClockType() {
  clock_type_ = (base::GetHourClockType() == base::k24HourClock)
                    ? base::k24HourClock
                    : base::k12HourClock;
  GetSystemTrayNotifier()->NotifyDateFormatChanged();
}

void SystemTrayDelegateCommon::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_UPGRADE_RECOMMENDED) {
    ash::UpdateInfo info;
    GetUpdateInfo(content::Source<UpgradeDetector>(source).ptr(), &info);
    GetSystemTrayNotifier()->NotifyUpdateRecommended(info);
  } else {
    NOTREACHED();
  }
}
