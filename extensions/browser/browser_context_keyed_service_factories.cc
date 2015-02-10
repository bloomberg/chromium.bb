// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_context_keyed_service_factories.h"

#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/networking_config/networking_config_service_factory.h"
#include "extensions/browser/api/networking_private/networking_private_event_router_factory.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_udp/udp_socket_event_dispatcher.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api/system_info/system_info_api.h"
#include "extensions/browser/api/usb/usb_event_router.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  AlarmManager::GetFactoryInstance();
  ApiResourceManager<ResumableTCPServerSocket>::GetFactoryInstance();
  ApiResourceManager<ResumableTCPSocket>::GetFactoryInstance();
  ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance();
  ApiResourceManager<SerialConnection>::GetFactoryInstance();
  ApiResourceManager<Socket>::GetFactoryInstance();
  AudioAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  chromeos::VpnServiceFactory::GetInstance();
#endif
  core_api::TCPServerSocketEventDispatcher::GetFactoryInstance();
  core_api::TCPSocketEventDispatcher::GetFactoryInstance();
  core_api::UDPSocketEventDispatcher::GetFactoryInstance();
  ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt();
  ExtensionPrefsFactory::GetInstance();
  HidDeviceManager::GetFactoryInstance();
  IdleManagerFactory::GetInstance();
  ManagementAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  NetworkingConfigServiceFactory::GetInstance();
#endif
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MACOSX)
  NetworkingPrivateEventRouterFactory::GetInstance();
#endif
  ProcessManagerFactory::GetInstance();
  RendererStartupHelperFactory::GetInstance();
  RuntimeAPI::GetFactoryInstance();
  StorageFrontend::GetFactoryInstance();
  SystemInfoAPI::GetFactoryInstance();
  UsbEventRouter::GetFactoryInstance();
}

}  // namespace extensions
