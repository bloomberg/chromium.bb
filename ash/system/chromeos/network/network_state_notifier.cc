// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace {

const int kMinTimeBetweenOutOfCreditsNotifySeconds = 10 * 60;

// Error messages based on |error_name|, not network_state->error().
string16 GetConnectErrorString(const std::string& error_name) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error_name == NetworkConnectionHandler::kErrorConfigureFailed)
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CONFIGURE_FAILED);
  if (error_name == NetworkConnectionHandler::kErrorActivateFailed)
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  return string16();
}

}  // namespace

namespace ash {

NetworkStateNotifier::NetworkStateNotifier()
    : cellular_out_of_credits_(false) {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);

  // Initialize |last_active_network_|.
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (default_network && default_network->IsConnectedState())
    last_active_network_ = default_network->path();
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
}

void NetworkStateNotifier::NetworkListChanged() {
  // Trigger any pending connect failed error if the network list changes
  // (which indicates all NetworkState entries are up to date). This is in
  // case a connect attempt fails because a network is no longer visible.
  if (!connect_failed_network_.empty()) {
    ShowNetworkConnectError(
        NetworkConnectionHandler::kErrorConnectFailed, connect_failed_network_);
  }
}

void NetworkStateNotifier::DefaultNetworkChanged(const NetworkState* network) {
  if (!network || !network->IsConnectedState())
    return;
  if (network->path() != last_active_network_) {
    last_active_network_ = network->path();
    // Reset state for new connected network
    cellular_out_of_credits_ = false;
  }
}

void NetworkStateNotifier::NetworkPropertiesUpdated(
    const NetworkState* network) {
  DCHECK(network);
  // Trigger a pending connect failed error for |network| when the Error
  // property has been set.
  if (network->path() == connect_failed_network_ && !network->error().empty()) {
    ShowNetworkConnectError(
        NetworkConnectionHandler::kErrorConnectFailed, connect_failed_network_);
  }
  // Trigger "Out of credits" notification if the cellular network is the most
  // recent default network (i.e. we have not switched to another network).
  if (network->type() == flimflam::kTypeCellular &&
      network->path() == last_active_network_) {
    cellular_network_ = network->path();
    if (network->cellular_out_of_credits() &&
        !cellular_out_of_credits_) {
      cellular_out_of_credits_ = true;
      base::TimeDelta dtime = base::Time::Now() - out_of_credits_notify_time_;
      if (dtime.InSeconds() > kMinTimeBetweenOutOfCreditsNotifySeconds) {
        out_of_credits_notify_time_ = base::Time::Now();
        std::vector<string16> links;
        links.push_back(
            l10n_util::GetStringFUTF16(IDS_NETWORK_OUT_OF_CREDITS_LINK,
                                       UTF8ToUTF16(network->name())));
        ash::Shell::GetInstance()->system_tray_notifier()->
            NotifySetNetworkMessage(
                this,
                NetworkObserver::ERROR_OUT_OF_CREDITS,
                NetworkObserver::GetNetworkTypeForNetworkState(network),
                l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_TITLE),
                l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_BODY),
                links);
      }
    }
  }
}

void NetworkStateNotifier::NotificationLinkClicked(
    NetworkObserver::MessageType message_type,
    size_t link_index) {
  if (message_type == NetworkObserver::ERROR_OUT_OF_CREDITS) {
    if (!cellular_network_.empty()) {
      // This will trigger the activation / portal code.
      Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
          cellular_network_);
    }
    ash::Shell::GetInstance()->system_tray_notifier()->
        NotifyClearNetworkMessage(message_type);
  }
}

void NetworkStateNotifier::ShowNetworkConnectError(
    const std::string& error_name,
    const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (error_name == NetworkConnectionHandler::kErrorConnectFailed &&
      service_path != connect_failed_network_) {
    // Shill may not have set the Error property yet. First request an update
    // and wait for either the update to complete or the network list to be
    // updated before displaying the error.
    connect_failed_network_ = service_path;
    return;
  }
  connect_failed_network_.clear();

  string16 error = GetConnectErrorString(error_name);
  if (error.empty() && network)
    error = network_connect::ErrorString(network->error());
  if (error.empty())
    error = l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  NET_LOG_ERROR("Connect error notification: " + UTF16ToUTF8(error),
                service_path);

  std::string name = network ? network->name() : "";
  string16 error_msg;
  if (network && !network->error_details().empty()) {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_SERVER_MESSAGE,
        UTF8ToUTF16(name), error, UTF8ToUTF16(network->error_details()));
  } else {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_DETAILS,
        UTF8ToUTF16(name), error);
  }

  std::vector<string16> no_links;
  ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
      this,
      NetworkObserver::ERROR_CONNECT_FAILED,
      NetworkObserver::GetNetworkTypeForNetworkState(network),
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE),
      error_msg,
      no_links);
}

}  // namespace ash
