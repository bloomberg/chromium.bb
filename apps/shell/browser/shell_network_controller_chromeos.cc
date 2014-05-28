// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_network_controller_chromeos.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace apps {

namespace {

// Frequency at which networks should be scanned when not connected.
const int kScanIntervalSec = 10;

void HandleEnableWifiError(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  LOG(WARNING) << "Unable to enable wifi: " << error_name;
}

// Returns a human-readable name for the network described by |state|.
std::string GetNetworkName(const chromeos::NetworkState& state) {
  return !state.name().empty() ? state.name() :
      base::StringPrintf("[%s]", state.type().c_str());
}

// Returns true if shill is either connected or connecting to a network.
bool IsConnectedOrConnecting() {
  chromeos::NetworkStateHandler* state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return state_handler->ConnectedNetworkByType(
             chromeos::NetworkTypePattern::Default()) ||
         state_handler->ConnectingNetworkByType(
             chromeos::NetworkTypePattern::Default());
}

}  // namespace

ShellNetworkController::ShellNetworkController()
    : waiting_for_connection_result_(false),
      weak_ptr_factory_(this) {
  chromeos::NetworkHandler::Initialize();
  chromeos::NetworkStateHandler* state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  DCHECK(state_handler);
  state_handler->AddObserver(this, FROM_HERE);
  state_handler->SetTechnologyEnabled(
      chromeos::NetworkTypePattern::Primitive(shill::kTypeWifi),
      true, base::Bind(&HandleEnableWifiError));

  if (!IsConnectedOrConnecting()) {
    RequestScan();
    SetScanningEnabled(true);
    ConnectIfUnconnected();
  }
}

ShellNetworkController::~ShellNetworkController() {
  chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
  chromeos::NetworkHandler::Shutdown();
}

void ShellNetworkController::NetworkListChanged() {
  VLOG(1) << "Network list changed";
  ConnectIfUnconnected();
}

void ShellNetworkController::DefaultNetworkChanged(
    const chromeos::NetworkState* state) {
  if (state) {
    VLOG(1) << "Default network state changed:"
            << " name=" << GetNetworkName(*state)
            << " path=" << state->path()
            << " state=" << state->connection_state();
  } else {
    VLOG(1) << "Default network state changed: [no network]";
  }

  if (IsConnectedOrConnecting()) {
    SetScanningEnabled(false);
  } else {
    SetScanningEnabled(true);
    ConnectIfUnconnected();
  }
}

void ShellNetworkController::SetScanningEnabled(bool enabled) {
  const bool currently_enabled = scan_timer_.IsRunning();
  if (enabled == currently_enabled)
    return;

  VLOG(1) << (enabled ? "Starting" : "Stopping") << " scanning";
  if (enabled) {
    scan_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(kScanIntervalSec),
                      this, &ShellNetworkController::RequestScan);
  } else {
    scan_timer_.Stop();
  }
}

void ShellNetworkController::RequestScan() {
  VLOG(1) << "Requesting scan";
  chromeos::NetworkHandler::Get()->network_state_handler()->RequestScan();
}

// Attempts to connect to an available network if not already connecting or
// connected.
void ShellNetworkController::ConnectIfUnconnected() {
  chromeos::NetworkHandler* handler = chromeos::NetworkHandler::Get();
  DCHECK(handler);
  if (waiting_for_connection_result_ || IsConnectedOrConnecting())
    return;

  chromeos::NetworkStateHandler::NetworkStateList state_list;
  handler->network_state_handler()->GetNetworkListByType(
      chromeos::NetworkTypePattern::WiFi(), &state_list);
  for (chromeos::NetworkStateHandler::NetworkStateList::const_iterator it =
       state_list.begin(); it != state_list.end(); ++it) {
    const chromeos::NetworkState* state = *it;
    DCHECK(state);
    if (!state->connectable())
      continue;

    VLOG(1) << "Connecting to network " << GetNetworkName(*state)
            << " with path " << state->path() << " and strength "
            << state->signal_strength();
    waiting_for_connection_result_ = true;
    handler->network_connection_handler()->ConnectToNetwork(
        state->path(),
        base::Bind(&ShellNetworkController::HandleConnectionSuccess,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ShellNetworkController::HandleConnectionError,
                   weak_ptr_factory_.GetWeakPtr()),
        false /* check_error_state */);

    return;
  }

  VLOG(1) << "Didn't find any connectable networks";
}

void ShellNetworkController::HandleConnectionSuccess() {
  VLOG(1) << "Successfully connected to network";
  waiting_for_connection_result_ = false;
}

void ShellNetworkController::HandleConnectionError(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  LOG(WARNING) << "Unable to connect to network: " << error_name;
  waiting_for_connection_result_ = false;
}

}  // namespace apps
