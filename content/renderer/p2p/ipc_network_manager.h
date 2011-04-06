// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
#define CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "third_party/libjingle/source/talk/base/network.h"

class P2PSocketDispatcher;

// IpcNetworkManager is a NetworkManager for libjingle that gets a
// list of network interfaces from the browser.
class IpcNetworkManager : public talk_base::NetworkManager {
 public:
  // Constructor doesn't take ownership of the |socket_dispatcher|.
  IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher);
  virtual ~IpcNetworkManager();

 protected:
  // Fills the supplied list with all usable networks.
  virtual bool EnumNetworks(bool include_ignored,
                            std::vector<talk_base::Network*>* networks)
      OVERRIDE;

  P2PSocketDispatcher* socket_dispatcher_;
};

#endif  // CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
