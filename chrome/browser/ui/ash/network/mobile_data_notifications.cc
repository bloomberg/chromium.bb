// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"

#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using session_manager::SessionManager;
using user_manager::UserManager;

namespace {

const char kMobileDataNotificationId[] =
    "chrome://settings/internet/mobile_data";
const char kNotifierMobileData[] = "ash.mobile-data";

void MobileDataNotificationClicked(const std::string& network_id) {
  SystemTrayClient::Get()->ShowNetworkSettings(network_id);
}

constexpr int kNotificationCheckDelayInSeconds = 2;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MobileDataNotifications

MobileDataNotifications::MobileDataNotifications() : weak_factory_(this) {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
  UserManager::Get()->AddSessionStateObserver(this);
  SessionManager::Get()->AddObserver(this);
}

MobileDataNotifications::~MobileDataNotifications() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
    NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
  }
  UserManager::Get()->RemoveSessionStateObserver(this);
  SessionManager::Get()->RemoveObserver(this);
}

void MobileDataNotifications::DefaultNetworkChanged(
    const NetworkState* default_network) {
  // No need to keep the timer running if we know default network is not
  // cellular.
  if (default_network && default_network->type() != shill::kTypeCellular) {
    one_shot_notification_check_delay_.Stop();
  }
  ShowOptionalMobileDataNotification();
}

void MobileDataNotifications::ConnectSucceeded(
    const std::string& service_path) {
  // We delay because it might take some time before the default network
  // changes after a connection is established.
  DelayedShowOptionalMobileDataNotification();
}

void MobileDataNotifications::ConnectFailed(const std::string& service_path,
                                            const std::string& error_name) {
  // We delay because it might take some time before the default network
  // changes after a connection request fails.
  DelayedShowOptionalMobileDataNotification();
}

void MobileDataNotifications::ActiveUserChanged(
    const user_manager::User* active_user) {
  ShowOptionalMobileDataNotification();
}

void MobileDataNotifications::OnSessionStateChanged() {
  ShowOptionalMobileDataNotification();
}

void MobileDataNotifications::ShowOptionalMobileDataNotification() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || default_network->type() != shill::kTypeCellular)
    return;
  if (SessionManager::Get()->IsUserSessionBlocked())
    return;
  if (NetworkHandler::Get()
          ->network_connection_handler()
          ->HasPendingConnectRequest()) {
    return;
  }

  // Check if we've shown this notification before.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kShowMobileDataNotification))
    return;

  // Prevent the notification from showing up in the future and stop any running
  // timers.
  prefs->SetBoolean(prefs::kShowMobileDataNotification, false);
  one_shot_notification_check_delay_.Stop();

  // Display a one-time notification on first use of Mobile Data connection.
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kMobileDataNotificationId,
          l10n_util::GetStringUTF16(IDS_MOBILE_DATA_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(IDS_3G_NOTIFICATION_MESSAGE),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierMobileData),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&MobileDataNotificationClicked,
                                  default_network->guid())),
          ash::kNotificationMobileDataIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void MobileDataNotifications::DelayedShowOptionalMobileDataNotification() {
  if (one_shot_notification_check_delay_.IsRunning()) {
    one_shot_notification_check_delay_.Reset();
    return;
  }
  one_shot_notification_check_delay_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kNotificationCheckDelayInSeconds),
      base::BindOnce(
          &MobileDataNotifications::ShowOptionalMobileDataNotification,
          // Callbacks won't run after this object is destroyed by using weak
          // pointers. Weak pointers are not thread safe but it's safe to use
          // here because timers run in a sequenced task runner.
          weak_factory_.GetWeakPtr()));
}
