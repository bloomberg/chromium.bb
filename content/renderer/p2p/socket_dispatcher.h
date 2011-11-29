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
//       P2PSocketHost                     P2PSocketClient
//            ^                                   ^
//            |                                   |
//            v                  IPC              v
//  P2PSocketDispatcherHost  <--------->  P2PSocketDispatcher
//

#ifndef CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
#define CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "content/common/p2p_sockets.h"
#include "content/public/renderer/render_view_observer.h"
#include "net/base/net_util.h"

class RenderViewImpl;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace net {
class IPEndPoint;
}  // namespace net

namespace content {

class P2PHostAddressRequest;
class P2PSocketClient;

// P2PSocketDispatcher works on the renderer thread. It dispatches all
// messages on that thread, and all its methods must be called on the
// same thread.
class P2PSocketDispatcher : public content::RenderViewObserver {
 public:
  class NetworkListObserver {
   public:
    virtual ~NetworkListObserver() { }

    virtual void OnNetworkListChanged(
        const net::NetworkInterfaceList& list) = 0;

   protected:
    NetworkListObserver() { }

   private:
    DISALLOW_COPY_AND_ASSIGN(NetworkListObserver);
  };

  explicit P2PSocketDispatcher(RenderViewImpl* render_view);
  virtual ~P2PSocketDispatcher();

  // Add a new network list observer. Each observer is called
  // immidiately after its't registered and then later whenever
  // network configuration changes.
  void AddNetworkListObserver(NetworkListObserver* network_list_observer);

  // Removes network list observer.
  void RemoveNetworkListObserver(NetworkListObserver* network_list_observer);

  // RenderViewObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  friend class P2PHostAddressRequest;
  friend class P2PSocketClient;
  class AsyncMessageSender;

  base::MessageLoopProxy* message_loop();

  // Called by P2PSocketClient.
  int RegisterClient(P2PSocketClient* client);
  void UnregisterClient(int id);
  void SendP2PMessage(IPC::Message* msg);

  // Called by DnsRequest.
  int RegisterHostAddressRequest(P2PHostAddressRequest* request);
  void UnregisterHostAddressRequest(int id);

  // Incoming message handlers.
  void OnNetworkListChanged(const net::NetworkInterfaceList& networks);
  void OnGetHostAddressResult(int32 request_id,
                              const net::IPAddressNumber& address);
  void OnSocketCreated(int socket_id, const net::IPEndPoint& address);
  void OnIncomingTcpConnection(int socket_id, const net::IPEndPoint& address);
  void OnError(int socket_id);
  void OnDataReceived(int socket_id, const net::IPEndPoint& address,
                      const std::vector<char>& data);

  P2PSocketClient* GetClient(int socket_id);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  IDMap<P2PSocketClient> clients_;

  IDMap<P2PHostAddressRequest> host_address_requests_;

  bool network_notifications_started_;
  scoped_refptr<ObserverListThreadSafe<NetworkListObserver> >
      network_list_observers_;

  scoped_refptr<AsyncMessageSender> async_message_sender_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
