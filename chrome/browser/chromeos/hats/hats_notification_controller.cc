// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_notification_controller.h"

#include "ash/system/system_notifier.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ash_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

// static
const char HatsNotificationController::kDelegateId[] = "hats_delegate";

// static
const char HatsNotificationController::kNotificationId[] = "hats_notification";

HatsNotificationController::HatsNotificationController(Profile* profile)
    : profile_(profile) {
  // Add self as an observer to be notified when an internet connection is
  // available.
  network_portal_detector::GetInstance()->AddAndFireObserver(this);
}

HatsNotificationController::~HatsNotificationController() {
  network_portal_detector::GetInstance()->RemoveObserver(this);
}

// static
// TODO(malaykeshav): Implement this stub.
bool HatsNotificationController::ShouldShowSurveyToProfile(Profile* profile) {
  // Do not show the survey if the HaTS feature is disabled for the device.
  return base::FeatureList::IsEnabled(features::kHappininessTrackingSystem);
}

// NotificationDelegate override:
std::string HatsNotificationController::id() const {
  return kDelegateId;
}

// message_center::NotificationDelegate override:
void HatsNotificationController::ButtonClick(int button_index) {}

// message_center::NotificationDelegate override:
void HatsNotificationController::Close(bool by_user) {}

// NetworkPortalDetector::Observer override:
void HatsNotificationController::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "HatsController::OnPortalDetectionCompleted(): "
          << "network=" << (network ? network->path() : "") << ", "
          << "state.status=" << state.status << ", "
          << "state.response_code=" << state.response_code;
  // Return if device is not connected to the internet.
  if (state.status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)
    return;
  // Remove self as an observer to no longer receive network change updates.
  network_portal_detector::GetInstance()->RemoveObserver(this);
  ShowNotification();
}

void HatsNotificationController::ShowNotification() {
  std::unique_ptr<Notification> notification(CreateNotification());
  g_browser_process->notification_ui_manager()->Add(*notification, profile_);
}

Notification* HatsNotificationController::CreateNotification() {
  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_TAKE_SURVEY_BUTTON)));

  return new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_BODY),
      // TODO(malaykeshav): Change this to actual HaTS icon.
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 ash::system_notifier::kNotifierHats),
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_HATS_NAME),
      GURL() /* Send an empty invalid url */, kNotificationId, optional, this);
}

}  // namespace chromeos
