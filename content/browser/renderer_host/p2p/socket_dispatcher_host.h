// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <map>

#include "content/browser/browser_message_filter.h"
#include "content/common/p2p_sockets.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"

namespace content {

class P2PSocketHost;
class ResourceContext;

class P2PSocketDispatcherHost
    : public BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  P2PSocketDispatcherHost(const content::ResourceContext* resource_context);
  virtual ~P2PSocketDispatcherHost();

  // BrowserMessageFilter overrides.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

 private:
  typedef std::pair<int32, int> ExtendedSocketId;
  typedef std::map<ExtendedSocketId, P2PSocketHost*> SocketsMap;

  class DnsRequest;

  P2PSocketHost* LookupSocket(int32 routing_id, int socket_id);

  // Handlers for the messages coming from the renderer.
  void OnStartNetworkNotifications(const IPC::Message& msg);
  void OnStopNetworkNotifications(const IPC::Message& msg);

  void OnGetHostAddress(const IPC::Message& msg,
                        const std::string& host_name,
                        int32 request_id);

  void OnCreateSocket(const IPC::Message& msg,
                      P2PSocketType type,
                      int socket_id,
                      const net::IPEndPoint& local_address,
                      const net::IPEndPoint& remote_address);
  void OnAcceptIncomingTcpConnection(const IPC::Message& msg,
                                     int listen_socket_id,
                                     const net::IPEndPoint& remote_address,
                                     int connected_socket_id);
  void OnSend(const IPC::Message& msg, int socket_id,
              const net::IPEndPoint& socket_address,
              const std::vector<char>& data);
  void OnDestroySocket(const IPC::Message& msg, int socket_id);

  void DoGetNetworkList();
  void SendNetworkList(const net::NetworkInterfaceList& list);

  void OnAddressResolved(DnsRequest* request,
                         const net::IPAddressNumber& result);

  const content::ResourceContext* resource_context_;

  SocketsMap sockets_;

  bool monitoring_networks_;

  // List or routing IDs for the hosts that have subscribed to the
  // network list notifications.
  std::set<int> notifications_routing_ids_;

  std::set<DnsRequest*> dns_requests_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
