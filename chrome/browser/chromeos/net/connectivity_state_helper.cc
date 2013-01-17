// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/connectivity_state_helper.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

static ConnectivityStateHelper* g_connectivity_state_helper = NULL;

// Implementation of the connectivity state helper that uses the network
// state handler for fetching connectivity state.
class ConnectivityStateHelperImpl
    : public ConnectivityStateHelper {
 public:
  ConnectivityStateHelperImpl();
  virtual ~ConnectivityStateHelperImpl();

  virtual bool IsConnectedType(const std::string& type) OVERRIDE;
  virtual bool IsConnectingType(const std::string& type) OVERRIDE;
  virtual std::string NetworkNameForType(const std::string& type) OVERRIDE;

 private:
  NetworkStateHandler* network_state_handler_;
};

// Implementation of the connectivity state helper that uses the network
// library for fetching connectivity state.
class ConnectivityStateHelperNetworkLibrary
    : public ConnectivityStateHelper {
 public:
  ConnectivityStateHelperNetworkLibrary();
  virtual ~ConnectivityStateHelperNetworkLibrary();

  virtual bool IsConnectedType(const std::string& type) OVERRIDE;
  virtual bool IsConnectingType(const std::string& type) OVERRIDE;
  virtual std::string NetworkNameForType(const std::string& type) OVERRIDE;

 private:
  NetworkLibrary* network_library_;
};

// static
void ConnectivityStateHelper::Initialize() {
  CHECK(!g_connectivity_state_helper);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableNewNetworkHandlers)) {
    g_connectivity_state_helper = new ConnectivityStateHelperImpl();
  } else {
    g_connectivity_state_helper =
        new ConnectivityStateHelperNetworkLibrary();
  }
}

// static
void ConnectivityStateHelper::Shutdown() {
  CHECK(g_connectivity_state_helper);
  delete g_connectivity_state_helper;
  g_connectivity_state_helper = NULL;
}

// static
ConnectivityStateHelper* ConnectivityStateHelper::Get() {
  DCHECK(g_connectivity_state_helper)
      << "ConnectivityStateHelper: Get() called before Initialize()";
  return g_connectivity_state_helper;
}

ConnectivityStateHelperImpl::ConnectivityStateHelperImpl() {
  network_state_handler_ = NetworkStateHandler::Get();
}

ConnectivityStateHelperImpl::~ConnectivityStateHelperImpl() {}

bool ConnectivityStateHelperImpl::IsConnectedType(
    const std::string& type) {
  return network_state_handler_->ConnectedNetworkByType(type) != NULL;
}

bool ConnectivityStateHelperImpl::IsConnectingType(
    const std::string& type) {
  return network_state_handler_->ConnectingNetworkByType(type) != NULL;
}

std::string ConnectivityStateHelperImpl::NetworkNameForType(
    const std::string& type) {
  const NetworkState* network = network_state_handler_->
      ConnectedNetworkByType(type);
  if (!network)
    network = network_state_handler_->ConnectingNetworkByType(type);
  return network ? network->name() : std::string();
}


ConnectivityStateHelperNetworkLibrary::ConnectivityStateHelperNetworkLibrary() {
  network_library_ = CrosLibrary::Get()->GetNetworkLibrary();
}

ConnectivityStateHelperNetworkLibrary::~ConnectivityStateHelperNetworkLibrary()
{}

bool ConnectivityStateHelperNetworkLibrary::IsConnectedType(
    const std::string& type) {
  if (type == flimflam::kTypeEthernet)
    return network_library_->ethernet_connected();
  if (type == flimflam::kTypeWifi)
    return network_library_->wifi_connected();
  if (type == flimflam::kTypeCellular)
    return network_library_->cellular_connected();
  if (type == flimflam::kTypeWimax)
    return network_library_->wimax_connected();
  return false;
}

bool ConnectivityStateHelperNetworkLibrary::IsConnectingType(
    const std::string& type) {
  if (type == flimflam::kTypeEthernet)
    return network_library_->ethernet_connecting();
  if (type == flimflam::kTypeWifi)
    return network_library_->wifi_connecting();
  if (type == flimflam::kTypeCellular)
    return network_library_->cellular_connecting();
  if (type == flimflam::kTypeWimax)
    return network_library_->cellular_connecting();
  return false;
}

std::string ConnectivityStateHelperNetworkLibrary::NetworkNameForType(
    const std::string& type) {
  if (type == flimflam::kTypeEthernet && network_library_->ethernet_network())
    return network_library_->ethernet_network()->name();
  if (type == flimflam::kTypeWifi && network_library_->wifi_network())
    return network_library_->wifi_network()->name();
  if (type == flimflam::kTypeCellular && network_library_->cellular_network())
    return network_library_->cellular_network()->name();
  if (type == flimflam::kTypeWimax && network_library_->wimax_network())
    return network_library_->wimax_network()->name();
  return std::string();
}

}  // namespace chromeos
