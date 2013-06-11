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
#include "base/values.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

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
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired) {
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
      false /* ignore_error_state */);
}

}  // network_connect
}  // ash
