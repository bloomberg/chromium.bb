// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <map>

#include "content/common/p2p_sockets.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"

namespace content {

class P2PSocketHost;
class ResourceContext;

class P2PSocketDispatcherHost
    : public content::BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  P2PSocketDispatcherHost(content::ResourceContext* resource_context);

  // content::BrowserMessageFilter overrides.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

 protected:
  virtual ~P2PSocketDispatcherHost();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<P2PSocketDispatcherHost>;

  typedef std::map<int, P2PSocketHost*> SocketsMap;

  class DnsRequest;

  P2PSocketHost* LookupSocket(int socket_id);

  // Handlers for the messages coming from the renderer.
  void OnStartNetworkNotifications(const IPC::Message& msg);
  void OnStopNetworkNotifications(const IPC::Message& msg);

  void OnGetHostAddress(const std::string& host_name,
                        int32 request_id);

  void OnCreateSocket(P2PSocketType type,
                      int socket_id,
                      const net::IPEndPoint& local_address,
                      const net::IPEndPoint& remote_address);
  void OnAcceptIncomingTcpConnection(int listen_socket_id,
                                     const net::IPEndPoint& remote_address,
                                     int connected_socket_id);
  void OnSend(int socket_id,
              const net::IPEndPoint& socket_address,
              const std::vector<char>& data);
  void OnDestroySocket(int socket_id);

  void DoGetNetworkList();
  void SendNetworkList(const net::NetworkInterfaceList& list);

  void OnAddressResolved(DnsRequest* request,
                         const net::IPAddressNumber& result);

  content::ResourceContext* resource_context_;

  SocketsMap sockets_;

  bool monitoring_networks_;

  std::set<DnsRequest*> dns_requests_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
