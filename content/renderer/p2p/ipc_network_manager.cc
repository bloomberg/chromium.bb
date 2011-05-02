// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include "net/base/net_util.h"
#include "net/base/sys_byteorder.h"
#include "content/renderer/p2p/socket_dispatcher.h"

// TODO(sergeyu): Currently the NetworkManager interface is
// syncronous, but it gets list of networks from the browser process
// asyncrhonously, so EnumNetworks() may return an empty list if we
// haven't received list of networks from the browser. Make
// NetworkManager interface asynchronous to avoid this problem.

IpcNetworkManager::IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher) {
  socket_dispatcher_->RequestNetworks();
}

IpcNetworkManager::~IpcNetworkManager() {
}

bool IpcNetworkManager::EnumNetworks(
    bool include_ignored, std::vector<talk_base::Network*>* networks) {
  socket_dispatcher_->RequestNetworks();
  net::NetworkInterfaceList list;
  socket_dispatcher_->GetNetworks(&list);
  for (net::NetworkInterfaceList::const_iterator it = list.begin();
       it != list.end(); it++) {
    uint32 address;
    if (it->address.size() != net::kIPv4AddressSize)
      continue;
    memcpy(&address, &it->address[0], sizeof(uint32));
    address = ntohl(address);
    networks->push_back(new talk_base::Network(it->name, it->name, address, 0));
  }
  return true;
}
