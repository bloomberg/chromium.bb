// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_network_monitor_private_impl.h"

#include "base/bind.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"

namespace content {
namespace {

bool AddNetworkListObserver(NetworkListObserver* observer) {
#if defined(ENABLE_WEBRTC)
  P2PSocketDispatcher* socket_dispatcher =
      RenderThreadImpl::current()->p2p_socket_dispatcher();
  if (!socket_dispatcher) {
    return false;
  }
  socket_dispatcher->AddNetworkListObserver(observer);
  return true;
#else
  return false;
#endif
}

void RemoveNetworkListObserver(NetworkListObserver* observer) {
#if defined(ENABLE_WEBRTC)
  P2PSocketDispatcher* socket_dispatcher =
      RenderThreadImpl::current()->p2p_socket_dispatcher();
  if (socket_dispatcher)
    socket_dispatcher->RemoveNetworkListObserver(observer);
#endif
}

}  // namespace

PPB_NetworkMonitor_Private_Impl::PPB_NetworkMonitor_Private_Impl(
    PP_Instance instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      callback_(callback),
      user_data_(user_data),
      started_(false) {
}

PPB_NetworkMonitor_Private_Impl::~PPB_NetworkMonitor_Private_Impl() {
  RemoveNetworkListObserver(this);
}

// static
PP_Resource PPB_NetworkMonitor_Private_Impl::Create(
    PP_Instance instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data) {
  scoped_refptr<PPB_NetworkMonitor_Private_Impl> result(
      new PPB_NetworkMonitor_Private_Impl(instance, callback, user_data));
  if (!result->Start())
    return 0;
  return result->GetReference();
}

ppapi::thunk::PPB_NetworkMonitor_Private_API*
PPB_NetworkMonitor_Private_Impl::AsPPB_NetworkMonitor_Private_API() {
  return this;
}

bool PPB_NetworkMonitor_Private_Impl::Start() {
  started_ = AddNetworkListObserver(this);
  return started_;
}

void PPB_NetworkMonitor_Private_Impl::OnNetworkListChanged(
    const net::NetworkInterfaceList& list) {
  ppapi::NetworkList list_copy(list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    ppapi::NetworkInfo& network = list_copy.at(i);
    network.name = list[i].name;

    network.addresses.resize(
        1, ppapi::NetAddressPrivateImpl::kInvalidNetAddress);
    bool result = ppapi::NetAddressPrivateImpl::IPEndPointToNetAddress(
        list[i].address, 0, &(network.addresses[0]));
    DCHECK(result);

    // TODO(sergeyu): Currently net::NetworkInterfaceList provides
    // only name and one IP address. Add all other fields and copy
    // them here.
    network.type = PP_NETWORKLIST_UNKNOWN;
    network.state = PP_NETWORKLIST_UP;
    network.display_name = list[i].name;
    network.mtu = 0;
  }
  scoped_refptr<ppapi::NetworkListStorage> list_storage(
      new ppapi::NetworkListStorage(list_copy));
  PP_Resource list_resource =
      ppapi::PPB_NetworkList_Private_Shared::Create(
          ppapi::OBJECT_IS_IMPL, pp_instance(), list_storage);
  callback_(user_data_, list_resource);
}

}  // namespace content
