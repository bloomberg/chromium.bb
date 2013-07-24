// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines local discovery messages between the browser and utility process.

#include "chrome/common/local_discovery/service_discovery_client.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START LocalDiscoveryMsgStart

IPC_STRUCT_TRAITS_BEGIN(local_discovery::ServiceDescription)
  IPC_STRUCT_TRAITS_MEMBER(service_name)
  IPC_STRUCT_TRAITS_MEMBER(address)
  IPC_STRUCT_TRAITS_MEMBER(metadata)
  IPC_STRUCT_TRAITS_MEMBER(ip_address)
  IPC_STRUCT_TRAITS_MEMBER(last_seen)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(local_discovery::ServiceWatcher::UpdateType)
IPC_ENUM_TRAITS(local_discovery::ServiceResolver::RequestStatus)
IPC_ENUM_TRAITS(net::AddressFamily)

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Creates watcher and starts listening in utility process.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_StartWatcher,
                     uint64 /* id */,
                     std::string /* service_type */)

// Discovers new services.
IPC_MESSAGE_CONTROL2(LocalDiscoveryMsg_DiscoverServices,
                     uint64 /* id */,
                     bool /* force_update */)

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

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

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
IPC_MESSAGE_CONTROL3(
    LocalDiscoveryHostMsg_LocalDomainResolverCallback,
    uint64 /* id */,
    bool /* success */,
    net::IPAddressNumber /* ip_address */)
