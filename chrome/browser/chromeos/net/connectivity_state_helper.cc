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
#include "chromeos/network/network_state_handler_observer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

static ConnectivityStateHelper* g_connectivity_state_helper = NULL;
static ConnectivityStateHelper* g_test_connectivity_state_helper = NULL;

// Implementation of the connectivity state helper that uses the network
// state handler for fetching connectivity state.
class ConnectivityStateHelperImpl
    : public ConnectivityStateHelper,
      public NetworkStateHandlerObserver {
 public:
  ConnectivityStateHelperImpl();
  virtual ~ConnectivityStateHelperImpl();

  // NetworkStateHandler overrides.
  virtual bool IsConnected() OVERRIDE;
  virtual bool IsConnecting() OVERRIDE;
  virtual bool IsConnectedType(const std::string& type) OVERRIDE;
  virtual bool IsConnectingType(const std::string& type) OVERRIDE;
  virtual std::string NetworkNameForType(const std::string& type) OVERRIDE;
  virtual std::string DefaultNetworkName() OVERRIDE;
  virtual bool DefaultNetworkOnline() OVERRIDE;
  virtual void RequestScan() const OVERRIDE;

  // NetworkStateHandlerObserver overrides.
  virtual void NetworkManagerChanged() OVERRIDE;
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;

 private:
  NetworkStateHandler* network_state_handler_;
};

// Implementation of the connectivity state helper that uses the network
// library for fetching connectivity state.
class ConnectivityStateHelperNetworkLibrary
    : public ConnectivityStateHelper,
      public NetworkLibrary::NetworkManagerObserver {
 public:
  ConnectivityStateHelperNetworkLibrary();
  virtual ~ConnectivityStateHelperNetworkLibrary();

  // ConnectivityStateHelper overrides.
  virtual bool IsConnected() OVERRIDE;
  virtual bool IsConnecting() OVERRIDE;
  virtual bool IsConnectedType(const std::string& type) OVERRIDE;
  virtual bool IsConnectingType(const std::string& type) OVERRIDE;
  virtual std::string NetworkNameForType(const std::string& type) OVERRIDE;
  virtual std::string DefaultNetworkName() OVERRIDE;
  virtual bool DefaultNetworkOnline() OVERRIDE;
  virtual void RequestScan() const OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver overrides.
  virtual void OnNetworkManagerChanged(
      NetworkLibrary* network_library) OVERRIDE;

 private:
  NetworkLibrary* network_library_;
};

ConnectivityStateHelper::ConnectivityStateHelper() {
}

ConnectivityStateHelper::~ConnectivityStateHelper() {
}

// static
void ConnectivityStateHelper::Initialize() {
  CHECK(!g_connectivity_state_helper);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableNewNetworkChangeNotifier)) {
    g_connectivity_state_helper = new ConnectivityStateHelperImpl();
  } else {
    g_connectivity_state_helper =
        new ConnectivityStateHelperNetworkLibrary();
  }
}

// static
bool ConnectivityStateHelper::IsInitialized() {
  return g_connectivity_state_helper != NULL;
}

// static
void ConnectivityStateHelper::Shutdown() {
  CHECK(g_connectivity_state_helper);
  delete g_connectivity_state_helper;
  g_connectivity_state_helper = NULL;
}

// static
ConnectivityStateHelper* ConnectivityStateHelper::Get() {
  CHECK(g_connectivity_state_helper || g_test_connectivity_state_helper)
      << "ConnectivityStateHelper: Get() called before Initialize()";
  if (g_test_connectivity_state_helper)
    return g_test_connectivity_state_helper;
  return g_connectivity_state_helper;
}

// static
void ConnectivityStateHelper::SetForTest(ConnectivityStateHelper* impl) {
  CHECK(!g_test_connectivity_state_helper || !impl);
  g_test_connectivity_state_helper = impl;
}

void ConnectivityStateHelper::AddNetworkManagerObserver(
    ConnectivityStateHelperObserver* observer) {
  connectivity_observers_.AddObserver(observer);
}

void ConnectivityStateHelper::RemoveNetworkManagerObserver(
    ConnectivityStateHelperObserver* observer) {
  connectivity_observers_.RemoveObserver(observer);
}

ConnectivityStateHelperImpl::ConnectivityStateHelperImpl() {
  network_state_handler_ = NetworkStateHandler::Get();
  network_state_handler_->AddObserver(this);
}

ConnectivityStateHelperImpl::~ConnectivityStateHelperImpl() {
  NetworkStateHandler::Get()->RemoveObserver(this);
}

bool ConnectivityStateHelperImpl::IsConnected() {
  return network_state_handler_->ConnectedNetworkByType(
      NetworkStateHandler::kMatchTypeDefault) != NULL;
}

bool ConnectivityStateHelperImpl::IsConnecting() {
  return network_state_handler_->ConnectingNetworkByType(
      NetworkStateHandler::kMatchTypeDefault) != NULL;
}

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

std::string ConnectivityStateHelperImpl::DefaultNetworkName() {
  const NetworkState* default_network = network_state_handler_->
      DefaultNetwork();
  return default_network ? default_network->name() : std::string();
}

bool ConnectivityStateHelperImpl::DefaultNetworkOnline() {
  const NetworkState* network = network_state_handler_->DefaultNetwork();
  if (!network)
    return false;
  if (!network->IsConnectedState())
    return false;
  if (network->connection_state() == flimflam::kStatePortal)
    return false;
  return true;
}

void ConnectivityStateHelperImpl::RequestScan() const {
  network_state_handler_->RequestScan();
}

void ConnectivityStateHelperImpl::NetworkManagerChanged() {
  FOR_EACH_OBSERVER(ConnectivityStateHelperObserver, connectivity_observers_,
                    NetworkManagerChanged());
}

void ConnectivityStateHelperImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  FOR_EACH_OBSERVER(ConnectivityStateHelperObserver, connectivity_observers_,
                    DefaultNetworkChanged());
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.
//

ConnectivityStateHelperNetworkLibrary::ConnectivityStateHelperNetworkLibrary() {
  network_library_ = CrosLibrary::Get()->GetNetworkLibrary();
  network_library_->AddNetworkManagerObserver(this);
}

ConnectivityStateHelperNetworkLibrary::~ConnectivityStateHelperNetworkLibrary()
{
  network_library_->RemoveNetworkManagerObserver(this);
}

bool ConnectivityStateHelperNetworkLibrary::IsConnected() {
  return network_library_->Connected();
}

bool ConnectivityStateHelperNetworkLibrary::IsConnecting() {
  return network_library_->Connecting();
}

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
    return network_library_->wimax_connecting();
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

std::string ConnectivityStateHelperNetworkLibrary::DefaultNetworkName() {
  if (network_library_->active_network())
    return network_library_->active_network()->name();
  return std::string();
}

bool ConnectivityStateHelperNetworkLibrary::DefaultNetworkOnline() {
  const Network* active_network = network_library_->active_network();
  if (!active_network)
    return false;
  if (!active_network->connected())
    return false;
  if (active_network->restricted_pool())
    return false;
  return true;
}

void ConnectivityStateHelperNetworkLibrary::RequestScan() const {
  network_library_->RequestNetworkScan();
}

void ConnectivityStateHelperNetworkLibrary::OnNetworkManagerChanged(
    NetworkLibrary* network_library) {
  FOR_EACH_OBSERVER(ConnectivityStateHelperObserver, connectivity_observers_,
                    NetworkManagerChanged());
}

}  // namespace chromeos
