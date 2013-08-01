// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_connect.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_state_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;

namespace ash {

namespace {

void OnConnectFailed(const std::string& service_path,
                     const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "Connect Failed for " << service_path << ": " << error_name;
  if (error_name == NetworkConnectionHandler::kErrorPassphraseRequired) {
    // TODO(stevenjb): Possibly add inline UI to handle passphrase entry here.
    ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
        service_path);
    return;
  }
  if (error_name == NetworkConnectionHandler::kErrorActivationRequired ||
      error_name == NetworkConnectionHandler::kErrorCertificateRequired ||
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired ||
      error_name == NetworkConnectionHandler::kErrorAuthenticationRequired) {
    ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
        service_path);
    return;
  }
  if (error_name == NetworkConnectionHandler::kErrorConnected ||
      error_name == NetworkConnectionHandler::kErrorConnecting) {
    ash::Shell::GetInstance()->system_tray_delegate()->ShowNetworkSettings(
        service_path);
    return;
  }
  // Shill does not always provide a helpful error. In this case, show the
  // configuration UI and a notification. See crbug.com/217033 for an example.
  if (error_name == NetworkConnectionHandler::kErrorConnectFailed) {
    ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
        service_path);
  }
  ash::Shell::GetInstance()->system_tray_notifier()->network_state_notifier()->
      ShowNetworkConnectError(error_name, service_path);
}

void OnConnectSucceeded(const std::string& service_path) {
  VLOG(1) << "Connect Succeeded for " << service_path;
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      NetworkObserver::ERROR_CONNECT_FAILED);
}

}  // namespace

namespace network_connect {

void ConnectToNetwork(const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network)
    return;
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      NetworkObserver::ERROR_CONNECT_FAILED);
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path,
      base::Bind(&OnConnectSucceeded, service_path),
      base::Bind(&OnConnectFailed, service_path),
      true /* check_error_state */);
}

string16 ErrorString(const std::string& error) {
  if (error.empty())
    return string16();
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
  if (error == flimflam::kErrorIpsecCertAuthFailed ||
      error == shill::kErrorEapAuthenticationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
  }
  if (error == shill::kErrorEapLocalTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_LOCAL_TLS_FAILED);
  }
  if (error == shill::kErrorEapRemoteTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_REMOTE_TLS_FAILED);
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

}  // network_connect
}  // ash
