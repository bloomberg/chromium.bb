// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_common.h"

#include "ash/networking_config_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/volume_control_delegate.h"
#include "base/time/time.h"
#include "build/build_config.h"
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

namespace {

ash::SystemTrayNotifier* GetSystemTrayNotifier() {
  return ash::Shell::GetInstance()->system_tray_notifier();
}

}  // namespace

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

void SystemTrayDelegateCommon::ShowChromeSlow() {
#if defined(OS_LINUX)
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
#endif  // defined(OS_LINUX)
}

void SystemTrayDelegateCommon::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetLastUsedProfile(),
                             chrome::HOST_DESKTOP_TYPE_ASH,
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayDelegateCommon::RequestRestartForUpdate() {
  chrome::AttemptRestart();
}

int SystemTrayDelegateCommon::GetSystemTrayMenuWidth() {
  return l10n_util::GetLocalizedContentsWidthInPixels(
      IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
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
