// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
#define CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_

#include <vector>

#include "base/task.h"
#include "base/compiler_specific.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/base/network.h"

namespace content {

// IpcNetworkManager is a NetworkManager for libjingle that gets a
// list of network interfaces from the browser.
class IpcNetworkManager : public talk_base::NetworkManagerBase,
                          public P2PSocketDispatcher::NetworkListObserver {
 public:
  // Constructor doesn't take ownership of the |socket_dispatcher|.
  IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher);
  virtual ~IpcNetworkManager();

  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // P2PSocketDispatcher::NetworkListObserver interface.
  virtual void OnNetworkListChanged(
      const net::NetworkInterfaceList& list) OVERRIDE;

 private:
  void SendNetworksChangedSignal();

  MessageLoop* message_loop_;
  P2PSocketDispatcher* socket_dispatcher_;
  bool started_;
  bool first_update_sent_;

  ScopedRunnableMethodFactory<IpcNetworkManager> task_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
