// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

using message_center::Notification;

namespace chromeos {

namespace {

bool IsPortalNotificationEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNetworkPortalNotification);
}


void CloseNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      NetworkPortalNotificationController::kNotificationId, false);
}

class NetworkPortalNotificationControllerDelegate
    : public message_center::NotificationDelegate {
 public:
  NetworkPortalNotificationControllerDelegate(): clicked_(false) {}

  // Overridden from message_center::NotificationDelegate:
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual void Click() OVERRIDE;

 private:
  virtual ~NetworkPortalNotificationControllerDelegate() {}

  bool clicked_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationControllerDelegate);
};

void NetworkPortalNotificationControllerDelegate::Display() {
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kNotificationMetric,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_DISPLAYED,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_COUNT);
}

void NetworkPortalNotificationControllerDelegate::Error() {
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kNotificationMetric,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_ERROR,
      NetworkPortalNotificationController::NOTIFICATION_METRIC_COUNT);
}

void NetworkPortalNotificationControllerDelegate::Close(bool by_user) {
  if (clicked_)
    return;
  NetworkPortalNotificationController::UserActionMetric metric =
      by_user
      ? NetworkPortalNotificationController::USER_ACTION_METRIC_CLOSED
      : NetworkPortalNotificationController::USER_ACTION_METRIC_IGNORED;
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kUserActionMetric,
      metric,
      NetworkPortalNotificationController::USER_ACTION_METRIC_COUNT);
}

void NetworkPortalNotificationControllerDelegate::Click() {
  clicked_ = true;
  UMA_HISTOGRAM_ENUMERATION(
      NetworkPortalNotificationController::kUserActionMetric,
      NetworkPortalNotificationController::USER_ACTION_METRIC_CLICKED,
      NetworkPortalNotificationController::USER_ACTION_METRIC_COUNT);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;
  chrome::ScopedTabbedBrowserDisplayer displayer(profile,
                                                 chrome::HOST_DESKTOP_TYPE_ASH);
  GURL url(captive_portal::CaptivePortalDetector::kDefaultURL);
  chrome::ShowSingletonTab(displayer.browser(), url);

  CloseNotification();
}

}  // namespace

// static
const char NetworkPortalNotificationController::kNotificationId[] =
    "chrome://net/network_portal_detector";

// static
const char NetworkPortalNotificationController::kNotificationMetric[] =
    "CaptivePortal.Notification.Status";

// static
const char NetworkPortalNotificationController::kUserActionMetric[] =
    "CaptivePortal.Notification.UserAction";

NetworkPortalNotificationController::NetworkPortalNotificationController() {}

NetworkPortalNotificationController::~NetworkPortalNotificationController() {}

void NetworkPortalNotificationController::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  if (!IsPortalNotificationEnabled())
    return;

  if (!network ||
      state.status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    last_network_path_.clear();
    CloseNotification();
    return;
  }

  // Don't do anything if notification for |network| already was
  // displayed.
  if (network->path() == last_network_path_)
    return;
  last_network_path_ = network->path();

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::Image& icon = bundle.GetImageNamed(IDR_PORTAL_DETECTION_ALERT);
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierNetworkPortalDetector);

  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId,
      l10n_util::GetStringUTF16(IDS_PORTAL_DETECTION_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE,
                                 base::UTF8ToUTF16(network->name())),
      icon,
      base::string16() /* display_source */,
      notifier_id,
      message_center::RichNotificationData(),
      new NetworkPortalNotificationControllerDelegate()));
  notification->SetSystemPriority();

  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()
        ->system_tray_notifier()
        ->NotifyOnCaptivePortalDetected(network->path());
  }

  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

}  // namespace chromeos
