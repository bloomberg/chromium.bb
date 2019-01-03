// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/mobile_data_notifications.h"

#include <memory>
#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
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
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;

namespace {

const char kMobileDataNotificationId[] =
    "chrome://settings/internet/mobile_data";
const char kNotifierMobileData[] = "ash.mobile-data";

void MobileDataNotificationClicked(const std::string& network_id) {
  SystemTrayClient::Get()->ShowNetworkSettings(network_id);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MobileDataNotifications

MobileDataNotifications::MobileDataNotifications() {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

MobileDataNotifications::~MobileDataNotifications() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

// TODO(tonydeluna): Rethink which events should trigger the mobile data warning
// notification. It should be enough to subscribe to default network changes and
// session changes.
void MobileDataNotifications::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!network || (network->type() != shill::kTypeCellular))
    return;
  ShowOptionalMobileDataNotification();
}

void MobileDataNotifications::DefaultNetworkChanged(
    const NetworkState* network) {
  // Call NetworkPropertiesUpdated in case the Cellular network became the
  // default network.
  NetworkPropertiesUpdated(network);
}

void MobileDataNotifications::ShowOptionalMobileDataNotification() {
  // Do not show notifications to unauthenticated users, or when requesting a
  // network connection, or if there's no default_network.
  if (!chromeos::LoginState::Get()->IsUserAuthenticated())
    return;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || default_network->type() != shill::kTypeCellular)
    return;
  if (NetworkHandler::Get()
          ->network_connection_handler()
          ->HasPendingConnectRequest())
    return;

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kShowMobileDataNotification))
    return;

  // Prevent the notification from showing up in the future.
  prefs->SetBoolean(prefs::kShowMobileDataNotification, false);

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
