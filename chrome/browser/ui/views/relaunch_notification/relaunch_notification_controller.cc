// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

// A type represending the possible RelaunchNotification policy setting values.
enum class RelaunchNotificationSetting {
  // Indications are via the Chrome menu only -- no work for the controller.
  kChromeMenuOnly,

  // Present the relaunch recommended bubble in the last active browser window.
  kRecommendedBubble,

  // Present the relaunch required dialog in the last active browser window.
  kRequiredDialog,
};

// Returns the policy setting, mapping out-of-range values to kChromeMenuOnly.
RelaunchNotificationSetting ReadPreference() {
  switch (g_browser_process->local_state()->GetInteger(
      prefs::kRelaunchNotification)) {
    case 1:
      return RelaunchNotificationSetting::kRecommendedBubble;
    case 2:
      return RelaunchNotificationSetting::kRequiredDialog;
  }
  return RelaunchNotificationSetting::kChromeMenuOnly;
}

}  // namespace

RelaunchNotificationController::RelaunchNotificationController(
    UpgradeDetector* upgrade_detector)
    : upgrade_detector_(upgrade_detector),
      last_notification_style_(NotificationStyle::kNone),
      last_level_(UpgradeDetector::UPGRADE_ANNOYANCE_NONE) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    // base::Unretained is safe here because |this| outlives the registrar.
    pref_change_registrar_.Add(
        prefs::kRelaunchNotification,
        base::BindRepeating(&RelaunchNotificationController::HandleCurrentStyle,
                            base::Unretained(this)));
    // Synchronize the instance with the current state of the preference.
    HandleCurrentStyle();
  }
}

RelaunchNotificationController::~RelaunchNotificationController() {
  if (last_notification_style_ != NotificationStyle::kNone)
    StopObservingUpgrades();
}

void RelaunchNotificationController::OnUpgradeRecommended() {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);
  UpgradeDetector::UpgradeNotificationAnnoyanceLevel current_level =
      upgrade_detector_->upgrade_notification_stage();

  // Nothing to do if there has been no change in the level. If appropriate, a
  // notification for this level has already been shown.
  if (current_level == last_level_)
    return;

  // Handle the new level.
  switch (current_level) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      // While it's unexpected that the level could move back down to none, it's
      // not a challenge to do the right thing.
      CloseRelaunchNotification();
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
    case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
      ShowRelaunchNotification();
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      // Critical notifications are handled by ToolbarView.
      // TODO(grt): Reconsider this when implementing the relaunch required
      // dialog. Obeying the administrator's wish to force a relaunch when
      // the annoyance level reaches HIGH is more important than showing the
      // critical update dialog. Perhaps handling of "critical" events should
      // be decoupled from the "relaunch to update" events.
      CloseRelaunchNotification();
      break;
  }

  last_level_ = current_level;
}

void RelaunchNotificationController::HandleCurrentStyle() {
  NotificationStyle notification_style = NotificationStyle::kNone;

  switch (ReadPreference()) {
    case RelaunchNotificationSetting::kChromeMenuOnly:
      DCHECK_EQ(notification_style, NotificationStyle::kNone);
      break;
    case RelaunchNotificationSetting::kRecommendedBubble:
      notification_style = NotificationStyle::kRecommended;
      break;
    case RelaunchNotificationSetting::kRequiredDialog:
      notification_style = NotificationStyle::kRequired;
      break;
  }

  // Nothing to do if there has been no change in the preference.
  if (notification_style == last_notification_style_)
    return;

  // Close the bubble or dialog if either is open.
  if (last_notification_style_ != NotificationStyle::kNone)
    CloseRelaunchNotification();

  // Reset state so that a notifications is shown anew in a new style if needed.
  last_level_ = UpgradeDetector::UPGRADE_ANNOYANCE_NONE;

  if (notification_style == NotificationStyle::kNone) {
    // Transition away from monitoring for upgrade events back to being dormant:
    // there is no need since AppMenuIconController takes care of updating the
    // Chrome menu as needed.
    StopObservingUpgrades();
    last_notification_style_ = notification_style;
    return;
  }

  // Transitioning away from being dormant: observe the UpgradeDetector.
  if (last_notification_style_ == NotificationStyle::kNone)
    StartObservingUpgrades();

  last_notification_style_ = notification_style;

  // Synchronize the instance with the current state of detection.
  OnUpgradeRecommended();
}

void RelaunchNotificationController::StartObservingUpgrades() {
  upgrade_detector_->AddObserver(this);
}

void RelaunchNotificationController::StopObservingUpgrades() {
  upgrade_detector_->RemoveObserver(this);
}

void RelaunchNotificationController::ShowRelaunchNotification() {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);

  if (last_notification_style_ == NotificationStyle::kRecommended)
    ShowRelaunchRecommendedBubble();
  else
    ShowRelaunchRequiredDialog();
}

void RelaunchNotificationController::CloseRelaunchNotification() {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);

  // Nothing needs to be closed if the annoyance level is none or critical.
  if (last_level_ == UpgradeDetector::UPGRADE_ANNOYANCE_NONE ||
      last_level_ == UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL) {
    return;
  }

  if (last_notification_style_ == NotificationStyle::kRecommended)
    CloseRelaunchRecommendedBubble();
  else
    CloseRelaunchRequiredDialog();
}

void RelaunchNotificationController::ShowRelaunchRecommendedBubble() {
  // TODO(grt): implement.
}

void RelaunchNotificationController::CloseRelaunchRecommendedBubble() {
  // TODO(grt): implement.
}

void RelaunchNotificationController::ShowRelaunchRequiredDialog() {
  // TODO(grt): implement.
}

void RelaunchNotificationController::CloseRelaunchRequiredDialog() {
  // TODO(grt): implement.
}
