// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/renderer_host/p2p/socket_host_throttler.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
class ProxyResolvingClientSocketFactory;
}

namespace content {

class P2PSocketHost;

class P2PSocketDispatcherHost
    : public base::RefCountedThreadSafe<P2PSocketDispatcherHost,
                                        BrowserThread::DeleteOnIOThread>,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public network::mojom::P2PSocketManager {
 public:
  P2PSocketDispatcherHost(int render_process_id,
                          net::URLRequestContextGetter* url_context);

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;
  // Starts the RTP packet header dumping. Must be called on the IO thread.
  void StartRtpDump(
      bool incoming,
      bool outgoing,
      const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback);

  // Stops the RTP packet header dumping. Must be Called on the UI thread.
  void StopRtpDumpOnUIThread(bool incoming, bool outgoing);

  void BindRequest(network::mojom::P2PSocketManagerRequest request);

  void AddAcceptedConnection(
      std::unique_ptr<P2PSocketHost> accepted_connection);
  void SocketDestroyed(P2PSocketHost* socket);

 private:
  friend class base::RefCountedThreadSafe<P2PSocketDispatcherHost>;
  friend class base::DeleteHelper<P2PSocketDispatcherHost>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  ~P2PSocketDispatcherHost() override;

  class DnsRequest;

  void DoGetNetworkList();
  void SendNetworkList(const net::NetworkInterfaceList& list,
                       const net::IPAddress& default_ipv4_local_address,
                       const net::IPAddress& default_ipv6_local_address);

  // network::mojom::P2PSocketManager overrides:
  void StartNetworkNotifications(
      network::mojom::P2PNetworkNotificationClientPtr client) override;
  void GetHostAddress(const std::string& host_name,
                      network::mojom::P2PSocketManager::GetHostAddressCallback
                          callback) override;
  void CreateSocket(network::P2PSocketType type,
                    const net::IPEndPoint& local_address,
                    const network::P2PPortRange& port_range,
                    const network::P2PHostAndIPEndPoint& remote_address,
                    network::mojom::P2PSocketClientPtr client,
                    network::mojom::P2PSocketRequest request) override;

  void NetworkNotificationClientConnectionError();

  // This connects a UDP socket to a public IP address and gets local
  // address. Since it binds to the "any" address (0.0.0.0 or ::) internally, it
  // retrieves the default local address.
  net::IPAddress GetDefaultLocalAddress(int family);

  void OnAddressResolved(
      DnsRequest* request,
      network::mojom::P2PSocketManager::GetHostAddressCallback callback,
      const net::IPAddressList& addresses);

  void StopRtpDumpOnIOThread(bool incoming, bool outgoing);

  int render_process_id_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  // Initialized on browser IO thread.
  std::unique_ptr<network::ProxyResolvingClientSocketFactory>
      proxy_resolving_socket_factory_;

  base::flat_map<P2PSocketHost*, std::unique_ptr<P2PSocketHost>> sockets_;

  std::set<std::unique_ptr<DnsRequest>> dns_requests_;
  P2PMessageThrottler throttler_;

  net::IPAddress default_ipv4_local_address_;
  net::IPAddress default_ipv6_local_address_;

  bool dump_incoming_rtp_packet_;
  bool dump_outgoing_rtp_packet_;
  RenderProcessHost::WebRtcRtpPacketCallback packet_callback_;

  // Used to call DoGetNetworkList, which may briefly block since getting the
  // default local address involves creating a dummy socket.
  const scoped_refptr<base::SequencedTaskRunner> network_list_task_runner_;

  mojo::Binding<network::mojom::P2PSocketManager> binding_;

  network::mojom::P2PNetworkNotificationClientPtr network_notification_client_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
