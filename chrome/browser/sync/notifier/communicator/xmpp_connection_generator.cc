// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#include "chrome/browser/sync/notifier/communicator/xmpp_connection_generator.h"

#include <vector>

#include "chrome/browser/sync/notifier/base/async_dns_lookup.h"
#include "chrome/browser/sync/notifier/base/signal_thread_task.h"
#include "chrome/browser/sync/notifier/communicator/connection_options.h"
#include "chrome/browser/sync/notifier/communicator/connection_settings.h"
#include "chrome/browser/sync/notifier/communicator/product_info.h"
#include "talk/base/autodetectproxy.h"
#include "talk/base/httpcommon.h"
#include "talk/base/logging.h"
#include "talk/base/task.h"
#include "talk/base/thread.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

XmppConnectionGenerator::XmppConnectionGenerator(
    talk_base::Task* parent,
    const ConnectionOptions* options,
    bool proxy_only,
    const ServerInformation* server_list,
    int server_count)
    : settings_list_(new ConnectionSettingsList()),
      settings_index_(0),
      server_list_(new ServerInformation[server_count]),
      server_count_(server_count),
      server_index_(-1),
      proxy_only_(proxy_only),
      successfully_resolved_dns_(false),
      first_dns_error_(0),
      options_(options),
      parent_(parent) {
  assert(parent);
  assert(options);
  assert(server_count_ > 0);
  for (int i = 0; i < server_count_; ++i) {
    server_list_[i] = server_list[i];
  }
}

XmppConnectionGenerator::~XmppConnectionGenerator() {
  LOG(LS_VERBOSE) << "XmppConnectionGenerator::~XmppConnectionGenerator";
}

const talk_base::ProxyInfo& XmppConnectionGenerator::proxy() const {
  assert(settings_list_.get());
  if (settings_index_ >= settings_list_->GetCount()) {
    return settings_list_->proxy();
  }

  ConnectionSettings* settings = settings_list_->GetSettings(settings_index_);
  return settings->proxy();
}

// Starts resolving proxy information.
void XmppConnectionGenerator::StartGenerating() {
  LOG(LS_VERBOSE) << "XmppConnectionGenerator::StartGenerating";

  talk_base::AutoDetectProxy* proxy_detect =
      new talk_base::AutoDetectProxy(GetUserAgentString());

  if (options_->autodetect_proxy()) {
    // Pretend the xmpp server is https, when detecting whether a proxy is
    // required to connect.
    talk_base::Url<char> host_url("/",
                                  server_list_[0].server.IPAsString().c_str(),
                                  server_list_[0].server.port());
    host_url.set_secure(true);
    proxy_detect->set_server_url(host_url.url());
  } else if (options_->proxy_host().length()) {
    talk_base::SocketAddress proxy(options_->proxy_host(),
                                   options_->proxy_port());
    proxy_detect->set_proxy(proxy);
  }
  proxy_detect->set_auth_info(options_->use_proxy_auth(),
                              options_->auth_user(),
                              talk_base::CryptString(options_->auth_pass()));

  SignalThreadTask<talk_base::AutoDetectProxy>* wrapper_task =
      new SignalThreadTask<talk_base::AutoDetectProxy>(parent_, &proxy_detect);
  wrapper_task->SignalWorkDone.connect(
      this,
      &XmppConnectionGenerator::OnProxyDetect);
  wrapper_task->Start();
}

void XmppConnectionGenerator::OnProxyDetect(
    talk_base::AutoDetectProxy* proxy_detect) {
  LOG(LS_VERBOSE) << "XmppConnectionGenerator::OnProxyDetect";

  ASSERT(settings_list_.get());
  ASSERT(proxy_detect);
  settings_list_->SetProxy(proxy_detect->proxy());

  // Start iterating through the connections (which are generated on demand).
  UseNextConnection();
}

void XmppConnectionGenerator::UseNextConnection() {
  // Trying to connect.

  // Iterate to the next possible connection.
  settings_index_++;
  if (settings_index_ < settings_list_->GetCount()) {
    // We have more connection settings in the settings_list_ to try, kick off
    // the next one.
    UseCurrentConnection();
    return;
  }

  // Iterate to the next possible server.
  server_index_++;
  if (server_index_ < server_count_) {
    AsyncDNSLookup* dns_lookup = new AsyncDNSLookup(
        server_list_[server_index_].server);
    SignalThreadTask<AsyncDNSLookup>* wrapper_task =
        new SignalThreadTask<AsyncDNSLookup>(parent_, &dns_lookup);
    wrapper_task->SignalWorkDone.connect(
        this,
        &XmppConnectionGenerator::OnServerDNSResolved);
    wrapper_task->Start();
    return;
  }

  // All out of possibilities.
  HandleExhaustedConnections();
}

void XmppConnectionGenerator::OnServerDNSResolved(
    AsyncDNSLookup* dns_lookup) {
  LOG(LS_VERBOSE) << "XmppConnectionGenerator::OnServerDNSResolved";

  // Print logging info.
  LOG(LS_VERBOSE) << "  server: " <<
      server_list_[server_index_].server.ToString() <<
      "  error: " << dns_lookup->error();
  if (first_dns_error_ == 0 && dns_lookup->error() != 0) {
    first_dns_error_ = dns_lookup->error();
  }

  if (!successfully_resolved_dns_ && dns_lookup->ip_list().size() > 0) {
    successfully_resolved_dns_ = true;
  }

  for (int i = 0; i < static_cast<int>(dns_lookup->ip_list().size()); ++i) {
    LOG(LS_VERBOSE)
        << "  ip " << i << " : "
        << talk_base::SocketAddress::IPToString(dns_lookup->ip_list()[i]);
  }

  // Build the ip list.
  assert(settings_list_.get());
  settings_index_ = -1;
  settings_list_->ClearPermutations();
  settings_list_->AddPermutations(
      server_list_[server_index_].server.IPAsString(),
      dns_lookup->ip_list(),
      server_list_[server_index_].server.port(),
      server_list_[server_index_].special_port_magic,
      proxy_only_);

  UseNextConnection();
}

static const char* const PROTO_NAMES[cricket::PROTO_LAST + 1] = {
  "udp", "tcp", "ssltcp"
};

static const char* ProtocolToString(cricket::ProtocolType proto) {
  return PROTO_NAMES[proto];
}

void XmppConnectionGenerator::UseCurrentConnection() {
  LOG(LS_VERBOSE) << "XmppConnectionGenerator::UseCurrentConnection";

  ConnectionSettings* settings = settings_list_->GetSettings(settings_index_);
  LOG(LS_INFO) << "*** Attempting "
               << ProtocolToString(settings->protocol()) << " connection to "
               << settings->server().IPAsString() << ":"
               << settings->server().port()
               << " (via " << ProxyToString(settings->proxy().type)
               << " proxy @ " << settings->proxy().address.IPAsString() << ":"
               << settings->proxy().address.port() << ")";

  SignalNewSettings(*settings);
}

void XmppConnectionGenerator::HandleExhaustedConnections() {
  LOG_F(LS_VERBOSE) << "(" << buzz::XmppEngine::ERROR_SOCKET
                    << ", " << first_dns_error_ << ")";
  SignalExhaustedSettings(successfully_resolved_dns_, first_dns_error_);
}

}  // namespace notifier
