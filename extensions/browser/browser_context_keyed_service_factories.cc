// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_context_keyed_service_factories.h"

#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_udp/udp_socket_event_dispatcher.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  ApiResourceManager<
      extensions::ResumableTCPServerSocket>::GetFactoryInstance();
  ApiResourceManager<extensions::ResumableTCPSocket>::GetFactoryInstance();
  ApiResourceManager<extensions::ResumableUDPSocket>::GetFactoryInstance();
  ApiResourceManager<extensions::SerialConnection>::GetFactoryInstance();
  ApiResourceManager<extensions::Socket>::GetFactoryInstance();
  core_api::TCPServerSocketEventDispatcher::GetFactoryInstance();
  core_api::TCPSocketEventDispatcher::GetFactoryInstance();
  core_api::UDPSocketEventDispatcher::GetFactoryInstance();
  ExtensionPrefsFactory::GetInstance();
  RendererStartupHelperFactory::GetInstance();
  RuntimeAPI::GetFactoryInstance();
  StorageFrontend::GetFactoryInstance();
}

}  // namespace extensions
