// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/p2p/ipc_network_manager.h"

IpcNetworkManager::IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher) {
}

IpcNetworkManager::~IpcNetworkManager() {
}

// TODO(sergeyu): Currently this method just adds one fake network in
// the list. This doesn't prevent PortAllocator from allocating ports:
// browser process chooses first IPv4-enabled interface. But this
// approach will not work in case when there is more than one active
// network interface. Implement this properly: get list of networks
// from the browser.
bool IpcNetworkManager::EnumNetworks(
    bool include_ignored, std::vector<talk_base::Network*>* networks) {
  networks->push_back(new talk_base::Network(
      "chrome", "Chrome virtual network", 0, 0));
  return true;
}
