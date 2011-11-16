// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "jingle/notifier/base/server_information.h"
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
    Delegate* delegate,
    net::HostResolver* host_resolver,
    const ConnectionOptions* options,
    bool try_ssltcp_first,
    const ServerList& servers)
    : delegate_(delegate),
      host_resolver_(host_resolver),
      settings_list_(new ConnectionSettingsList()),
      settings_index_(0),
      servers_(servers),
      current_server_(servers_.end()),
      try_ssltcp_first_(try_ssltcp_first),
      successfully_resolved_dns_(false),
      first_dns_error_(0),
      should_resolve_dns_(true),
      options_(options) {
  DCHECK(delegate_);
  DCHECK(host_resolver);
  DCHECK(options_);
  DCHECK_GT(servers_.size(), 0u);
}

XmppConnectionGenerator::~XmppConnectionGenerator() {
  VLOG(1) << "XmppConnectionGenerator::~XmppConnectionGenerator";
}

// Starts resolving proxy information.
void XmppConnectionGenerator::StartGenerating() {
  VLOG(1) << "XmppConnectionGenerator::StartGenerating";

  // TODO(akalin): Detect proxy settings once we use Chrome sockets.

  // Start iterating through the connections (which are generated on demand).
  UseNextConnection();
}

namespace {

const char* const PROTO_NAMES[cricket::PROTO_LAST + 1] = {
  "udp", "tcp", "ssltcp"
};

}  // namespace

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
      ConnectionSettings* settings =
          settings_list_->GetSettings(settings_index_);
      VLOG(1) << "*** Attempting " << PROTO_NAMES[settings->protocol()]
              << " connection to " << settings->server().IPAsString()
              << ":" << settings->server().port();
      delegate_->OnNewSettings(*settings);
      return;
    }

    // Iterate to the next possible server.
    if (current_server_ == servers_.end()) {
      current_server_ = servers_.begin();
    } else {
      ++current_server_;
    }
    if (current_server_ == servers_.end()) {
      // All out of possibilities.
      VLOG(1) << "(" << buzz::XmppEngine::ERROR_SOCKET
              << ", " << first_dns_error_ << ")";
      delegate_->OnExhaustedSettings(
          successfully_resolved_dns_, first_dns_error_);
      return;
    }

    if (should_resolve_dns_) {
      // Resolve the server.
      const net::HostPortPair& server = current_server_->server;
      net::HostResolver::RequestInfo request_info(server);
      int status =
          host_resolver_.Resolve(
              request_info, &address_list_,
              base::Bind(&XmppConnectionGenerator::OnServerDNSResolved,
                         base::Unretained(this)),
              bound_net_log_);
      if (status == net::ERR_IO_PENDING)  // OnServerDNSResolved will be called.
        return;

      HandleServerDNSResolved(status);
    } else {
      // We are not resolving DNS here (DNS will be resolved by a lower layer).
      // Generate settings using an empty IP list (which will just use the
      // host name for the current server).
      std::vector<uint32> ip_list;
      GenerateSettingsForIPList(ip_list);
    }
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
  VLOG(1) << "XmppConnectionGenerator::HandleServerDNSResolved";
  // Print logging info.
  VLOG(1) << "  server: " << current_server_->server.ToString()
          << ", error: " << status;
  if (status != net::OK) {
    if (first_dns_error_ == 0)
      first_dns_error_ = status;
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
    VLOG(1) << "  ip " << i
            << " : " << talk_base::SocketAddress::IPToString(ip_list[i]);
  }

  GenerateSettingsForIPList(ip_list);
}

void XmppConnectionGenerator::GenerateSettingsForIPList(
    const std::vector<uint32>& ip_list) {
  // Build the ip list.
  DCHECK(settings_list_.get());
  settings_index_ = -1;
  settings_list_->ClearPermutations();
  settings_list_->AddPermutations(
      current_server_->server.host(),
      ip_list,
      current_server_->server.port(),
      current_server_->special_port_magic,
      try_ssltcp_first_);
}

}  // namespace notifier
