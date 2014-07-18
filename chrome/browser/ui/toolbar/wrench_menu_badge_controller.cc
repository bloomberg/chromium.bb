// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_badge_controller.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/upgrade_detector.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

namespace {

// Maps an upgrade level to a severity level.
WrenchIconPainter::Severity SeverityFromUpgradeLevel(
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
  switch (level) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      return WrenchIconPainter::SEVERITY_NONE;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      return WrenchIconPainter::SEVERITY_LOW;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      return WrenchIconPainter::SEVERITY_MEDIUM;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return WrenchIconPainter::SEVERITY_HIGH;
    case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
      return WrenchIconPainter::SEVERITY_HIGH;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      return WrenchIconPainter::SEVERITY_HIGH;
  }
  NOTREACHED();
  return WrenchIconPainter::SEVERITY_NONE;
}

// Checks if the wrench icon should be animated for the given upgrade level.
bool ShouldAnimateUpgradeLevel(
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
  bool should_animate = true;
  if (level == UpgradeDetector::UPGRADE_ANNOYANCE_LOW) {
    // Only animate low severity upgrades once.
    static bool should_animate_low_severity = true;
    should_animate = should_animate_low_severity;
    should_animate_low_severity = false;
  }
  return should_animate;
}

// Returns true if we should show the upgrade recommended badge.
bool ShouldShowUpgradeRecommended() {
#if defined(OS_CHROMEOS)
  // In chromeos, the update recommendation is shown in the system tray. So it
  // should not be displayed in the wrench menu.
  return false;
#else
  return UpgradeDetector::GetInstance()->notify_upgrade();
#endif
}

// Returns true if we should show the warning for incompatible software.
bool ShouldShowIncompatibilityWarning() {
#if defined(OS_WIN)
  EnumerateModulesModel* loaded_modules = EnumerateModulesModel::GetInstance();
  loaded_modules->MaybePostScanningTask();
  return loaded_modules->ShouldShowConflictWarning();
#else
  return false;
#endif
}

}  // namespace

WrenchMenuBadgeController::WrenchMenuBadgeController(Profile* profile,
                                                     Delegate* delegate)
    : profile_(profile), delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);

  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(profile_));

#if defined(OS_WIN)
  if (base::win::GetVersion() == base::win::VERSION_XP) {
    registrar_.Add(this, chrome::NOTIFICATION_MODULE_LIST_ENUMERATED,
                   content::NotificationService::AllSources());
  }
  registrar_.Add(this, chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_BADGE_CHANGE,
                 content::NotificationService::AllSources());
#endif
}

WrenchMenuBadgeController::~WrenchMenuBadgeController() {
}

void WrenchMenuBadgeController::UpdateDelegate() {
  if (ShouldShowUpgradeRecommended()) {
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level =
        UpgradeDetector::GetInstance()->upgrade_notification_stage();
    delegate_->UpdateBadgeSeverity(BADGE_TYPE_UPGRADE_NOTIFICATION,
                                   SeverityFromUpgradeLevel(level),
                                   ShouldAnimateUpgradeLevel(level));
    return;
  }

  if (ShouldShowIncompatibilityWarning()) {
    delegate_->UpdateBadgeSeverity(BADGE_TYPE_INCOMPATIBILITY_WARNING,
                                   WrenchIconPainter::SEVERITY_MEDIUM, true);
    return;
  }

  if (GlobalErrorServiceFactory::GetForProfile(profile_)->
          GetHighestSeverityGlobalErrorWithWrenchMenuItem()) {
    // If you change the severity here, make sure to also change the menu icon
    // and the bubble icon.
    delegate_->UpdateBadgeSeverity(BADGE_TYPE_GLOBAL_ERROR,
                                   WrenchIconPainter::SEVERITY_MEDIUM, true);
    return;
  }

  delegate_->UpdateBadgeSeverity(BADGE_TYPE_NONE,
                                 WrenchIconPainter::SEVERITY_NONE, true);
}

void WrenchMenuBadgeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  UpdateDelegate();
}
