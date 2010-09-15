// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// XmppConnectionGenerator does the following algorithm:
//   proxy = ResolveProxyInformation(connection_options)
//   for server in server_list
//     get dns_addresses for server
//     connection_list = (dns_addresses X connection methods X proxy).shuffle()
//     for connection in connection_list
//       yield connection

#include "jingle/notifier/communicator/xmpp_connection_generator.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#endif

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "talk/base/httpcommon-inl.h"
#include "talk/base/task.h"
#include "talk/base/thread.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

XmppConnectionGenerator::XmppConnectionGenerator(
    talk_base::Task* parent,
    const scoped_refptr<net::HostResolver>& host_resolver,
    const ConnectionOptions* options,
    bool try_ssltcp_first,
    bool proxy_only,
    const ServerInformation* server_list,
    int server_count)
    : host_resolver_(host_resolver),
      resolve_callback_(
          ALLOW_THIS_IN_INITIALIZER_LIST(
              NewCallback(this,
                          &XmppConnectionGenerator::OnServerDNSResolved))),
      settings_list_(new ConnectionSettingsList()),
      settings_index_(0),
      server_list_(new ServerInformation[server_count]),
      server_count_(server_count),
      server_index_(-1),
      try_ssltcp_first_(try_ssltcp_first),
      proxy_only_(proxy_only),
      successfully_resolved_dns_(false),
      first_dns_error_(0),
      options_(options),
      parent_(parent) {
  DCHECK(host_resolver);
  DCHECK(parent);
  DCHECK(options);
  DCHECK_GT(server_count_, 0);
  for (int i = 0; i < server_count_; ++i) {
    server_list_[i] = server_list[i];
  }
}

XmppConnectionGenerator::~XmppConnectionGenerator() {
  LOG(INFO) << "XmppConnectionGenerator::~XmppConnectionGenerator";
}

const talk_base::ProxyInfo& XmppConnectionGenerator::proxy() const {
  DCHECK(settings_list_.get());
  if (settings_index_ >= settings_list_->GetCount()) {
    return settings_list_->proxy();
  }

  ConnectionSettings* settings = settings_list_->GetSettings(settings_index_);
  return settings->proxy();
}

// Starts resolving proxy information.
void XmppConnectionGenerator::StartGenerating() {
  LOG(INFO) << "XmppConnectionGenerator::StartGenerating";

  // TODO(akalin): Detect proxy settings once we use Chrome sockets.

  // Start iterating through the connections (which are generated on demand).
  UseNextConnection();
}

void XmppConnectionGenerator::UseNextConnection() {
  DCHECK(settings_list_.get());
  // Loop until we can use a connection or we run out of connections
  // to try.
  while (true) {
    // Iterate to the next possible connection.
    settings_index_++;
    if (settings_index_ < settings_list_->GetCount()) {
      // We have more connection settings in the settings_list_ to
      // try, kick off the next one.
      UseCurrentConnection();
      return;
    }

    // Iterate to the next possible server.
    server_index_++;
    if (server_index_ >= server_count_) {
      // All out of possibilities.
      HandleExhaustedConnections();
      return;
    }

    // Resolve the server.
    const net::HostPortPair& server =
        server_list_[server_index_].server;
    net::HostResolver::RequestInfo request_info(server);
    int status =
        host_resolver_.Resolve(
            request_info, &address_list_, resolve_callback_.get(),
            bound_net_log_);
    if (status == net::ERR_IO_PENDING) {
      // resolve_callback_ will call us when it's called.
      return;
    }
    HandleServerDNSResolved(status);
  }
}

void XmppConnectionGenerator::OnServerDNSResolved(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  HandleServerDNSResolved(status);
  // Reenter loop.
  UseNextConnection();
}

void XmppConnectionGenerator::HandleServerDNSResolved(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  LOG(INFO) << "XmppConnectionGenerator::HandleServerDNSResolved";
  // Print logging info.
  LOG(INFO) << "  server: "
            << server_list_[server_index_].server.ToString()
            << ", error: " << status;
  if (status != net::OK) {
    if (first_dns_error_ == 0) {
      first_dns_error_ = status;
    }
    return;
  }

  // Slurp the addresses into a vector.
  std::vector<uint32> ip_list;
  for (const addrinfo* addr = address_list_.head();
       addr != NULL; addr = addr->ai_next) {
    const sockaddr_in& sockaddr =
        *reinterpret_cast<const sockaddr_in*>(addr->ai_addr);
    uint32 ip = ntohl(sockaddr.sin_addr.s_addr);
    ip_list.push_back(ip);
  }
  successfully_resolved_dns_ = !ip_list.empty();

  for (int i = 0; i < static_cast<int>(ip_list.size()); ++i) {
    LOG(INFO)
        << "  ip " << i << " : "
        << talk_base::SocketAddress::IPToString(ip_list[i]);
  }

  // Build the ip list.
  DCHECK(settings_list_.get());
  settings_index_ = -1;
  settings_list_->ClearPermutations();
  settings_list_->AddPermutations(
      server_list_[server_index_].server.host(),
      ip_list,
      server_list_[server_index_].server.port(),
      server_list_[server_index_].special_port_magic,
      try_ssltcp_first_,
      proxy_only_);
}

static const char* const PROTO_NAMES[cricket::PROTO_LAST + 1] = {
  "udp", "tcp", "ssltcp"
};

static const char* ProtocolToString(cricket::ProtocolType proto) {
  return PROTO_NAMES[proto];
}

void XmppConnectionGenerator::UseCurrentConnection() {
  LOG(INFO) << "XmppConnectionGenerator::UseCurrentConnection";

  ConnectionSettings* settings = settings_list_->GetSettings(settings_index_);
  LOG(INFO) << "*** Attempting "
            << ProtocolToString(settings->protocol()) << " connection to "
            << settings->server().IPAsString() << ":"
            << settings->server().port()
            << " (via " << ProxyToString(settings->proxy().type)
            << " proxy @ " << settings->proxy().address.IPAsString() << ":"
            << settings->proxy().address.port() << ")";

  SignalNewSettings(*settings);
}

void XmppConnectionGenerator::HandleExhaustedConnections() {
  LOG(INFO) << "(" << buzz::XmppEngine::ERROR_SOCKET
            << ", " << first_dns_error_ << ")";
  SignalExhaustedSettings(successfully_resolved_dns_, first_dns_error_);
}

}  // namespace notifier
