// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/task/post_task_forward.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/public/cpp/p2p_param_traits.h"

using content::BrowserMessageFilter;
using content::BrowserThread;

namespace content {

namespace {

// Used by GetDefaultLocalAddress as the target to connect to for getting the
// default local address. They are public DNS servers on the internet.
const uint8_t kPublicIPv4Host[] = {8, 8, 8, 8};
const uint8_t kPublicIPv6Host[] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
const int kPublicPort = 53;  // DNS port.

// Experimentation shows that creating too many sockets creates odd problems
// because of resource exhaustion in the Unix sockets domain.
// Trouble has been seen on Linux at 3479 sockets in test, so leave a margin.
const int kMaxSimultaneousSockets = 3000;

}  // namespace

class P2PSocketDispatcherHost::DnsRequest {
 public:
  typedef base::Callback<void(const net::IPAddressList&)> DoneCallback;

  explicit DnsRequest(net::HostResolver* host_resolver)
      : resolver_(host_resolver) {}

  void Resolve(const std::string& host_name,
               const DoneCallback& done_callback) {
    DCHECK(!done_callback.is_null());

    host_name_ = host_name;
    done_callback_ = done_callback;

    // Return an error if it's an empty string.
    if (host_name_.empty()) {
      net::IPAddressList address_list;
      done_callback_.Run(address_list);
      return;
    }

    // Add period at the end to make sure that we only resolve
    // fully-qualified names.
    if (host_name_.back() != '.')
      host_name_ += '.';

    net::HostResolver::RequestInfo info(net::HostPortPair(host_name_, 0));
    int result = resolver_->Resolve(
        info, net::DEFAULT_PRIORITY, &addresses_,
        base::BindOnce(&P2PSocketDispatcherHost::DnsRequest::OnDone,
                       base::Unretained(this)),
        &request_, net::NetLogWithSource());
    if (result != net::ERR_IO_PENDING)
      OnDone(result);
  }

 private:
  void OnDone(int result) {
    net::IPAddressList list;
    if (result != net::OK) {
      LOG(ERROR) << "Failed to resolve address for " << host_name_
                 << ", errorcode: " << result;
      done_callback_.Run(list);
      return;
    }

    DCHECK(!addresses_.empty());
    for (net::AddressList::iterator iter = addresses_.begin();
         iter != addresses_.end(); ++iter) {
      list.push_back(iter->address());
    }
    done_callback_.Run(list);
  }

  net::AddressList addresses_;

  std::string host_name_;
  net::HostResolver* resolver_;
  std::unique_ptr<net::HostResolver::Request> request_;

  DoneCallback done_callback_;
};

P2PSocketDispatcherHost::P2PSocketDispatcherHost(
    int render_process_id,
    net::URLRequestContextGetter* url_context)
    : render_process_id_(render_process_id),
      url_context_(url_context),
      dump_incoming_rtp_packet_(false),
      dump_outgoing_rtp_packet_(false),
      network_list_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      binding_(this) {}

void P2PSocketDispatcherHost::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // NetworkChangeNotifier always emits CONNECTION_NONE notification whenever
  // network configuration changes. All other notifications can be ignored.
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  // Notify the renderer about changes to list of network interfaces.
  network_list_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&P2PSocketDispatcherHost::DoGetNetworkList, this));
}

void P2PSocketDispatcherHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if ((!dump_incoming_rtp_packet_ && incoming) ||
      (!dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = true;

    if (outgoing)
      dump_outgoing_rtp_packet_ = true;

    packet_callback_ = packet_callback;
    for (auto& it : sockets_)
      it.first->StartRtpDump(incoming, outgoing, packet_callback);
  }
}

void P2PSocketDispatcherHost::StopRtpDumpOnUIThread(bool incoming,
                                                    bool outgoing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&P2PSocketDispatcherHost::StopRtpDumpOnIOThread, this,
                     incoming, outgoing));
}

void P2PSocketDispatcherHost::BindRequest(
    network::mojom::P2PSocketManagerRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void P2PSocketDispatcherHost::AddAcceptedConnection(
    std::unique_ptr<P2PSocketHost> accepted_connection) {
  sockets_[accepted_connection.get()] = std::move(accepted_connection);
}

void P2PSocketDispatcherHost::SocketDestroyed(P2PSocketHost* socket) {
  auto iter = sockets_.find(socket);
  if (iter == sockets_.end())
    return;  // This condition is reached in this class's destructor.

  iter->second.release();
  sockets_.erase(iter);
}

P2PSocketDispatcherHost::~P2PSocketDispatcherHost() {
  // Make local copy so that SocketDestroyed doesn't try to delete again.
  auto sockets = std::move(sockets_);
  sockets.clear();
  dns_requests_.clear();

  if (network_notification_client_)
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);

  proxy_resolving_socket_factory_.reset();
}

void P2PSocketDispatcherHost::DoGetNetworkList() {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetNetworkList failed.";
    return;
  }
  default_ipv4_local_address_ = GetDefaultLocalAddress(AF_INET);
  default_ipv6_local_address_ = GetDefaultLocalAddress(AF_INET6);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&P2PSocketDispatcherHost::SendNetworkList, this, list,
                     default_ipv4_local_address_, default_ipv6_local_address_));
}

void P2PSocketDispatcherHost::SendNetworkList(
    const net::NetworkInterfaceList& list,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  network_notification_client_->NetworkListChanged(
      list, default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketDispatcherHost::StartNetworkNotifications(
    network::mojom::P2PNetworkNotificationClientPtr client) {
  DCHECK(!network_notification_client_);
  network_notification_client_ = std::move(client);
  network_notification_client_.set_connection_error_handler(base::BindOnce(
      &P2PSocketDispatcherHost::NetworkNotificationClientConnectionError,
      base::Unretained(this)));

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  network_list_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&P2PSocketDispatcherHost::DoGetNetworkList, this));
}

void P2PSocketDispatcherHost::GetHostAddress(
    const std::string& host_name,
    network::mojom::P2PSocketManager::GetHostAddressCallback callback) {
  std::unique_ptr<DnsRequest> request = std::make_unique<DnsRequest>(
      url_context_->GetURLRequestContext()->host_resolver());
  DnsRequest* request_ptr = request.get();
  dns_requests_.insert(std::move(request));
  request_ptr->Resolve(
      host_name,
      base::Bind(&P2PSocketDispatcherHost::OnAddressResolved,
                 base::Unretained(this), request_ptr, base::Passed(&callback)));
}

void P2PSocketDispatcherHost::CreateSocket(
    network::P2PSocketType type,
    const net::IPEndPoint& local_address,
    const network::P2PPortRange& port_range,
    const network::P2PHostAndIPEndPoint& remote_address,
    network::mojom::P2PSocketClientPtr client,
    network::mojom::P2PSocketRequest request) {
  if (port_range.min_port > port_range.max_port ||
      (port_range.min_port == 0 && port_range.max_port != 0)) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::SDH_INVALID_PORT_RANGE);
    return;
  }

  if (!proxy_resolving_socket_factory_) {
    proxy_resolving_socket_factory_ =
        std::make_unique<network::ProxyResolvingClientSocketFactory>(
            url_context_->GetURLRequestContext());
  }
  if (sockets_.size() > kMaxSimultaneousSockets) {
    LOG(ERROR) << "Too many sockets created";
    return;
  }
  std::unique_ptr<P2PSocketHost> socket(P2PSocketHost::Create(
      this, std::move(client), std::move(request), type, url_context_.get(),
      proxy_resolving_socket_factory_.get(), &throttler_));

  if (!socket)
    return;

  if (socket->Init(local_address, port_range.min_port, port_range.max_port,
                   remote_address)) {
    if (dump_incoming_rtp_packet_ || dump_outgoing_rtp_packet_) {
      socket->StartRtpDump(dump_incoming_rtp_packet_, dump_outgoing_rtp_packet_,
                           packet_callback_);
    }
    sockets_[socket.get()] = std::move(socket);
  }
}

void P2PSocketDispatcherHost::NetworkNotificationClientConnectionError() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

net::IPAddress P2PSocketDispatcherHost::GetDefaultLocalAddress(int family) {
  DCHECK(family == AF_INET || family == AF_INET6);

  // Creation and connection of a UDP socket might be janky.
  DCHECK(network_list_task_runner_->RunsTasksInCurrentSequence());

  auto socket =
      net::ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource());

  net::IPAddress ip_address;
  if (family == AF_INET) {
    ip_address = net::IPAddress(kPublicIPv4Host);
  } else {
    ip_address = net::IPAddress(kPublicIPv6Host);
  }

  if (socket->Connect(net::IPEndPoint(ip_address, kPublicPort)) != net::OK) {
    return net::IPAddress();
  }

  net::IPEndPoint local_address;
  if (socket->GetLocalAddress(&local_address) != net::OK)
    return net::IPAddress();

  return local_address.address();
}

void P2PSocketDispatcherHost::OnAddressResolved(
    DnsRequest* request,
    network::mojom::P2PSocketManager::GetHostAddressCallback callback,
    const net::IPAddressList& addresses) {
  std::move(callback).Run(addresses);

  dns_requests_.erase(
      std::find_if(dns_requests_.begin(), dns_requests_.end(),
                   [request](const std::unique_ptr<DnsRequest>& ptr) {
                     return ptr.get() == request;
                   }));
}

void P2PSocketDispatcherHost::StopRtpDumpOnIOThread(bool incoming,
                                                    bool outgoing) {
  if ((dump_incoming_rtp_packet_ && incoming) ||
      (dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = false;

    if (outgoing)
      dump_outgoing_rtp_packet_ = false;

    if (!dump_incoming_rtp_packet_ && !dump_outgoing_rtp_packet_)
      packet_callback_.Reset();

    for (auto& it : sockets_)
      it.first->StopRtpDump(incoming, outgoing);
  }
}

}  // namespace content
