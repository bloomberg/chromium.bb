// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// P2PSocketDispatcher is a per-renderer object that dispatchers all
// P2P messages received from the browser and relays all P2P messages
// sent to the browser. P2PSocketClient instances register themselves
// with the dispatcher using RegisterClient() and UnregisterClient().
//
// Relationship of classes.
//
//      P2PSocketHost                 P2PSocketClient
//           ^                               ^
//           |                               |
//           v              IPC              v
//      P2PSocketsHost  <--------->  P2PSocketDispatcher
//

#ifndef CHROME_RENDERER_P2P_SOCKET_DISPATCHER_H_
#define CHROME_RENDERER_P2P_SOCKET_DISPATCHER_H_

#include <vector>

#include "base/id_map.h"
#include "chrome/renderer/p2p/socket_client.h"
#include "content/common/p2p_sockets.h"
#include "chrome/renderer/render_view_observer.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

// P2PSocketDispatcher works on the renderer thread. It dispatches all
// messages on that thread, and all its methods must be called on the
// same thread.
class P2PSocketDispatcher : public RenderViewObserver {
 public:
  explicit P2PSocketDispatcher(RenderView* render_view);
  virtual ~P2PSocketDispatcher();

  P2PSocketClient* CreateSocket(P2PSocketType type,
                                const net::IPEndPoint& address,
                                P2PSocketClient::Delegate* delegate);

  // RenderViewObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  friend class P2PSocketClient;

  // Called by P2PSocketClient.
  int RegisterClient(P2PSocketClient* client);
  void UnregisterClient(int id);
  void SendP2PMessage(IPC::Message* msg);
  base::MessageLoopProxy* message_loop();

  // Incoming message handlers.
  void OnSocketCreated(int socket_id, const net::IPEndPoint& address);
  void OnError(int socket_id);
  void OnDataReceived(int socket_id, const net::IPEndPoint& address,
                      const std::vector<char>& data);

  P2PSocketClient* GetClient(int socket_id);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  IDMap<P2PSocketClient> clients_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcher);
};

#endif  // CHROME_RENDERER_P2P_SOCKET_DISPATCHER_H_
