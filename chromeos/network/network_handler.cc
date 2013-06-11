// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/cert_loader.h"
#include "chromeos/network/geolocation_handler.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_profile_observer.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

static NetworkHandler* g_network_handler = NULL;

NetworkHandler::NetworkHandler() {
  CHECK(DBusThreadManager::IsInitialized());

  network_event_log::Initialize();

  cert_loader_.reset(new CertLoader);
  network_state_handler_.reset(new NetworkStateHandler());
  network_profile_handler_.reset(new NetworkProfileHandler());
  network_configuration_handler_.reset(new NetworkConfigurationHandler());
  managed_network_configuration_handler_.reset(
      new ManagedNetworkConfigurationHandler());
  network_connection_handler_.reset(new NetworkConnectionHandler());
  geolocation_handler_.reset(new GeolocationHandler());
}

NetworkHandler::~NetworkHandler() {
  network_event_log::Shutdown();
}

void NetworkHandler::Init() {
  network_state_handler_->InitShillPropertyHandler();
  network_configuration_handler_->Init(network_state_handler_.get());
  managed_network_configuration_handler_->Init(
      network_state_handler_.get(),
      network_profile_handler_.get(),
      network_configuration_handler_.get());
  network_connection_handler_->Init(cert_loader_.get(),
                                    network_state_handler_.get(),
                                    network_configuration_handler_.get());
  geolocation_handler_->Init();
}

// static
void NetworkHandler::Initialize() {
  CHECK(!g_network_handler);
  g_network_handler = new NetworkHandler();
  g_network_handler->Init();
}

// static
void NetworkHandler::Shutdown() {
  CHECK(g_network_handler);
  delete g_network_handler;
  g_network_handler = NULL;
}

// static
NetworkHandler* NetworkHandler::Get() {
  CHECK(g_network_handler)
      << "NetworkHandler::Get() called before Initialize()";
  return g_network_handler;
}

// static
bool NetworkHandler::IsInitialized() {
  return g_network_handler;
}

CertLoader* NetworkHandler::cert_loader() {
  return cert_loader_.get();
}

NetworkStateHandler* NetworkHandler::network_state_handler() {
  return network_state_handler_.get();
}

NetworkProfileHandler* NetworkHandler::network_profile_handler() {
  return network_profile_handler_.get();
}

NetworkConfigurationHandler* NetworkHandler::network_configuration_handler() {
  return network_configuration_handler_.get();
}

ManagedNetworkConfigurationHandler*
NetworkHandler::managed_network_configuration_handler() {
  return managed_network_configuration_handler_.get();
}

NetworkConnectionHandler* NetworkHandler::network_connection_handler() {
  return network_connection_handler_.get();
}

GeolocationHandler* NetworkHandler::geolocation_handler() {
  return geolocation_handler_.get();
}

}  // namespace chromeos
