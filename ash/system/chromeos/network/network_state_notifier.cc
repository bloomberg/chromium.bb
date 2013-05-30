// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace {

const int kMinTimeBetweenOutOfCreditsNotifySeconds = 10 * 60;

string16 GetErrorString(const std::string& error) {
  if (error == flimflam::kErrorOutOfRange)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
  if (error == flimflam::kErrorPinMissing)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
  if (error == flimflam::kErrorDhcpFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
  if (error == flimflam::kErrorConnectFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error == flimflam::kErrorBadPassphrase)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  if (error == flimflam::kErrorBadWEPKey)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
  if (error == flimflam::kErrorActivationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error == flimflam::kErrorNeedEvdo)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
  if (error == flimflam::kErrorNeedHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
  }
  if (error == flimflam::kErrorOtaspFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
  if (error == flimflam::kErrorAaaFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
  if (error == flimflam::kErrorInternal)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
  if (error == flimflam::kErrorDNSLookupFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
  }
  if (error == flimflam::kErrorHTTPGetFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  if (error == flimflam::kErrorIpsecPskAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
  }
  if (error == flimflam::kErrorIpsecCertAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_CERT_AUTH_FAILED);
  }
  if (error == flimflam::kErrorPppAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
  }
  if (StringToLowerASCII(error) ==
      StringToLowerASCII(std::string(flimflam::kUnknownString))) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  return l10n_util::GetStringFUTF16(IDS_NETWORK_UNRECOGNIZED_ERROR,
                                    UTF8ToUTF16(error));
}

}  // namespace

namespace ash {

NetworkStateNotifier::NetworkStateNotifier()
    : cellular_out_of_credits_(false) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this);
    InitializeNetworks();
  }
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
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

void NetworkStateNotifier::NetworkConnectionStateChanged(
    const NetworkState* network) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  std::string prev_state;
  std::string new_state = network->connection_state();
  CachedStateMap::iterator iter = cached_state_.find(network->path());
  if (iter != cached_state_.end()) {
    prev_state = iter->second;
    if (prev_state == new_state)
      return;  // No state change
    VLOG(1) << "NetworkStateNotifier: State: " << prev_state
            << " ->: " << new_state;
    iter->second = new_state;
  } else {
    VLOG(1) << "NetworkStateNotifier: New Service: " << network->path()
            << " State: " << new_state;
    cached_state_[network->path()] = new_state;
    return;  // New network, no state change
  }

  if (new_state != flimflam::kStateFailure)
    return;

  if (network->path() != handler->connecting_network())
    return;  // Only show notifications for explicitly connected networks

  NET_LOG_EVENT("ConnectionFailure", network->path());

  std::vector<string16> no_links;
  string16 error = GetErrorString(network->error());
  ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
      this,
      NetworkObserver::ERROR_CONNECT_FAILED,
      NetworkObserver::GetNetworkTypeForNetworkState(network),
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE),
      l10n_util::GetStringFUTF16(
            IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_DETAILS,
            UTF8ToUTF16(network->name()), error),
      no_links);
}

void NetworkStateNotifier::NetworkPropertiesUpdated(
    const NetworkState* network) {
  DCHECK(network);
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

void NetworkStateNotifier::InitializeNetworks() {
  NetworkStateList network_list;
  NetworkHandler::Get()->network_state_handler()->GetNetworkList(&network_list);
  VLOG(1) << "NetworkStateNotifier:InitializeNetworks: "
          << network_list.size();
  for (NetworkStateList::iterator iter = network_list.begin();
       iter != network_list.end(); ++iter) {
    const NetworkState* network = *iter;
    VLOG(2) << " Network: " << network->path();
    cached_state_[network->path()] = network->connection_state();
  }
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (default_network && default_network->IsConnectedState())
    last_active_network_ = default_network->path();
}

}  // namespace ash
