// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines local discovery messages between the browser and utility process.

// Multiple-included file, no traditional include guard.
#include <vector>

#include "chrome/common/local_discovery/service_discovery_client.h"
#include "ipc/ipc_message_macros.h"

#ifndef CHROME_COMMON_LOCAL_DISCOVERY_LOCAL_DISCOVERY_MESSAGES_H_
#define CHROME_COMMON_LOCAL_DISCOVERY_LOCAL_DISCOVERY_MESSAGES_H_

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_POSIX)
struct LocalDiscoveryMsg_SocketInfo {
  LocalDiscoveryMsg_SocketInfo()
      : address_family(net::ADDRESS_FAMILY_UNSPECIFIED),
        interface_index(0) {
  }

  base::FileDescriptor descriptor;
  net::AddressFamily address_family;
  uint32 interface_index;
};
#endif  // OS_POSIX

#endif  // CHROME_COMMON_LOCAL_DISCOVERY_LOCAL_DISCOVERY_MESSAGES_H_

#define IPC_MESSAGE_START LocalDiscoveryMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(local_discovery::ServiceWatcher::UpdateType,
                          local_discovery::ServiceWatcher::UPDATE_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(
    local_discovery::ServiceResolver::RequestStatus,
    local_discovery::ServiceResolver::REQUEST_STATUS_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(net::AddressFamily, net::ADDRESS_FAMILY_LAST)

IPC_STRUCT_TRAITS_BEGIN(local_discovery::ServiceDescription)
  IPC_STRUCT_TRAITS_MEMBER(service_name)
  IPC_STRUCT_TRAITS_MEMBER(address)
  IPC_STRUCT_TRAITS_MEMBER(metadata)
  IPC_STRUCT_TRAITS_MEMBER(ip_address)
  IPC_STRUCT_TRAITS_MEMBER(last_seen)
IPC_STRUCT_TRAITS_END()

#if defined(OS_POSIX)
IPC_STRUCT_TRAITS_BEGIN(LocalDiscoveryMsg_SocketInfo)
  IPC_STRUCT_TRAITS_MEMBER(descriptor)
  IPC_STRUCT_TRAITS_MEMBER(address_family)
  IPC_STRUCT_TRAITS_MEMBER(interface_index)
IPC_STRUCT_TRAITS_END()
#endif  // OS_POSIX
//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

#if defined(OS_POSIX)
IPC_MESSAGE_CONTROL1(LocalDiscoveryMsg_SetSockets,
                     std::vector<LocalDiscoveryMsg_SocketInfo> /* sockets */)
#endif  // OS_POSIX

// Creates watcher and starts listening in utility process.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_StartWatcher,
                     uint64 /* id */,
                     std::string /* service_type */)

// Discovers new services.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_DiscoverServices,
                     uint64 /* id */,
                     bool /* force_update */)

// Discovers new services.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_SetActivelyRefreshServices,
                     uint64 /* id */,
                     bool /* actively_refresh_services */)

// Destroys watcher in utility process.
IPC_MESSAGE_CONTROL1(LocalDiscoveryMsg_DestroyWatcher,
                     uint64 /* id */)

// Creates service resolver and starts resolving service in utility process.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_ResolveService,
                     uint64 /* id */,
                     std::string /* service_name */)

// Destroys service resolver in utility process.
IPC_MESSAGE_CONTROL1(LocalDiscoveryMsg_DestroyResolver,
                     uint64 /* id */)

// Creates a local domain resolver and starts resolving in utility process.
IPC_MESSAGE_CONTROL3(LocalDiscoveryMsg_ResolveLocalDomain,
                     uint64 /* id */,
                     std::string /* domain */,
                     net::AddressFamily /* address_family */)

// Destroys local domain resolver in utility process.
IPC_MESSAGE_CONTROL1(LocalDiscoveryMsg_DestroyLocalDomainResolver,
                     uint64 /* id */)

// Stops local discovery in utility process. http://crbug.com/268466.
IPC_MESSAGE_CONTROL0(LocalDiscoveryMsg_ShutdownLocalDiscovery)


//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Notifies browser process if process failed.
IPC_MESSAGE_CONTROL0(LocalDiscoveryHostMsg_Error)

// Notifies browser process about new services.
IPC_MESSAGE_CONTROL3(LocalDiscoveryHostMsg_WatcherCallback,
                     uint64 /* id */,
                     local_discovery::ServiceWatcher::UpdateType /* update */,
                     std::string /* service_name */)

// Notifies browser process about service resolution results.
IPC_MESSAGE_CONTROL3(
    LocalDiscoveryHostMsg_ResolverCallback,
    uint64 /* id */,
    local_discovery::ServiceResolver::RequestStatus /* status */,
    local_discovery::ServiceDescription /* description */)

// Notifies browser process about local domain resolution results.
IPC_MESSAGE_CONTROL4(
    LocalDiscoveryHostMsg_LocalDomainResolverCallback,
    uint64 /* id */,
    bool /* success */,
    net::IPAddressNumber /* ip_address_ipv4 */,
    net::IPAddressNumber /* ip_address_ipv6 */)
