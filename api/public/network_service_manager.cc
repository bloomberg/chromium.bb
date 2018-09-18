// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/network_service_manager.h"

#include "api/impl/internal_services.h"

namespace {

openscreen::NetworkServiceManager* g_network_service_manager_instance = nullptr;

}  //  namespace

namespace openscreen {

// static
NetworkServiceManager* NetworkServiceManager::Create(
    std::unique_ptr<ScreenListener> mdns_listener,
    std::unique_ptr<ScreenPublisher> mdns_publisher,
    std::unique_ptr<ScreenConnectionClient> connection_client,
    std::unique_ptr<ScreenConnectionServer> connection_server) {
  // TODO(mfoltz): Convert to assertion failure
  if (g_network_service_manager_instance)
    return nullptr;
  g_network_service_manager_instance = new NetworkServiceManager(
      std::move(mdns_listener), std::move(mdns_publisher),
      std::move(connection_client), std::move(connection_server));
  return g_network_service_manager_instance;
}

// static
NetworkServiceManager* NetworkServiceManager::Get() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance)
    return nullptr;
  return g_network_service_manager_instance;
}

// static
void NetworkServiceManager::Dispose() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance)
    return;
  delete g_network_service_manager_instance;
  g_network_service_manager_instance = nullptr;
}

void NetworkServiceManager::RunEventLoopOnce() {
  InternalServices::RunEventLoopOnce();
}

ScreenListener* NetworkServiceManager::GetMdnsScreenListener() {
  return mdns_listener_.get();
}

ScreenPublisher* NetworkServiceManager::GetMdnsScreenPublisher() {
  return mdns_publisher_.get();
}

ScreenConnectionClient* NetworkServiceManager::GetScreenConnectionClient() {
  return connection_client_.get();
}

ScreenConnectionServer* NetworkServiceManager::GetScreenConnectionServer() {
  return connection_server_.get();
}

NetworkServiceManager::NetworkServiceManager(
    std::unique_ptr<ScreenListener> mdns_listener,
    std::unique_ptr<ScreenPublisher> mdns_publisher,
    std::unique_ptr<ScreenConnectionClient> connection_client,
    std::unique_ptr<ScreenConnectionServer> connection_server)
    : mdns_listener_(std::move(mdns_listener)),
      mdns_publisher_(std::move(mdns_publisher)),
      connection_client_(std::move(connection_client)),
      connection_server_(std::move(connection_server)) {}

NetworkServiceManager::~NetworkServiceManager() = default;

}  // namespace openscreen
