// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include "base/bind.h"
#include "net/base/net_util.h"
#include "net/base/sys_byteorder.h"

namespace content {

IpcNetworkManager::IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher),
      started_(false),
      first_update_sent_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

IpcNetworkManager::~IpcNetworkManager() {
  socket_dispatcher_->RemoveNetworkListObserver(this);
}

void IpcNetworkManager::StartUpdating() {
  if (!started_) {
    first_update_sent_ = false;
    started_ = true;
    socket_dispatcher_->AddNetworkListObserver(this);
  } else {
    // Post a task to avoid reentrancy.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&IpcNetworkManager::SendNetworksChangedSignal,
                              weak_factory_.GetWeakPtr()));
  }
}

void IpcNetworkManager::StopUpdating() {
  started_ = false;
  socket_dispatcher_->RemoveNetworkListObserver(this);
}

void IpcNetworkManager::OnNetworkListChanged(
    const net::NetworkInterfaceList& list) {
  if (!started_)
    return;

  std::vector<talk_base::Network*> networks;
  for (net::NetworkInterfaceList::const_iterator it = list.begin();
       it != list.end(); it++) {
    uint32 address;
    if (it->address.size() != net::kIPv4AddressSize)
      continue;
    memcpy(&address, &it->address[0], sizeof(uint32));
    address = ntohl(address);
    networks.push_back(
        new talk_base::Network(it->name, it->name, address));
  }

  MergeNetworkList(networks, !first_update_sent_);
  first_update_sent_ = false;
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
