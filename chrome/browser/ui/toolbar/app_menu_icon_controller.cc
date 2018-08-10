// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace {

// Maps an upgrade level to a severity level. When |show_very_low_upgrade_level|
// is true, VERY_LOW through HIGH all return Severity::LOW. Otherwise, VERY_LOW
// is ignored and LOW through HIGH return their respective Severity level.
AppMenuIconController::Severity SeverityFromUpgradeLevel(
    bool show_very_low_upgrade_level,
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
  if (show_very_low_upgrade_level) {
    // Anything between kNone and kCritical is LOW for unstable desktop Chrome.
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
        break;
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
        return AppMenuIconController::Severity::LOW;
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::HIGH;
    }
  } else {
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
        break;
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
        // kVeryLow is meaningless for stable channels.
        return AppMenuIconController::Severity::NONE;
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
        return AppMenuIconController::Severity::LOW;
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
        return AppMenuIconController::Severity::MEDIUM;
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::HIGH;
    }
  }
  DCHECK_EQ(level, UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  return AppMenuIconController::Severity::NONE;
}

// Checks if the app menu icon should be animated for the given severity.
bool ShouldAnimateSeverity(AppMenuIconController::Severity severity) {
  return severity != AppMenuIconController::Severity::NONE;
}

// Returns true if we should show the upgrade recommended icon.
bool ShouldShowUpgradeRecommended() {
#if defined(OS_CHROMEOS)
  // In chromeos, the update recommendation is shown in the system tray. So it
  // should not be displayed in the app menu.
  return false;
#else
  return UpgradeDetector::GetInstance()->notify_upgrade();
#endif
}

// Return true if the browser is updating on the dev or canary channels.
bool IsUnstableChannel() {
  // Unbranded (Chromium) builds are on the UNKNOWN channel, so check explicitly
  // for the Google Chrome channels that are considered "unstable". This ensures
  // that Chromium builds get the default behavior.
  const version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::DEV ||
         channel == version_info::Channel::CANARY;
}

}  // namespace

AppMenuIconController::AppMenuIconController(Profile* profile,
                                             Delegate* delegate)
    : is_unstable_channel_(IsUnstableChannel()),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);

  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(profile_));

  UpgradeDetector::GetInstance()->AddObserver(this);
}

AppMenuIconController::~AppMenuIconController() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

void AppMenuIconController::UpdateDelegate() {
  if (ShouldShowUpgradeRecommended()) {
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level =
        UpgradeDetector::GetInstance()->upgrade_notification_stage();
    auto severity = SeverityFromUpgradeLevel(is_unstable_channel_, level);
    delegate_->UpdateSeverity(IconType::UPGRADE_NOTIFICATION, severity,
                              ShouldAnimateSeverity(severity));
    return;
  }

  if (GlobalErrorServiceFactory::GetForProfile(profile_)
          ->GetHighestSeverityGlobalErrorWithAppMenuItem()) {
    // If you change the severity here, make sure to also change the menu icon
    // and the bubble icon.
    delegate_->UpdateSeverity(IconType::GLOBAL_ERROR,
                              Severity::MEDIUM, true);
    return;
  }

  delegate_->UpdateSeverity(IconType::NONE, Severity::NONE, false);
}

void AppMenuIconController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED, type);
  UpdateDelegate();
}

void AppMenuIconController::OnUpgradeRecommended() {
  UpdateDelegate();
}
