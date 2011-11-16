// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/single_request_host_resolver.h"
#include "jingle/notifier/base/server_information.h"

namespace notifier {

class ConnectionOptions;
class ConnectionSettings;
class ConnectionSettingsList;

// Resolves dns names and iterates through the various ip address and transport
// combinations.
class XmppConnectionGenerator {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnNewSettings(const ConnectionSettings& new_settings) = 0;
    virtual void OnExhaustedSettings(bool successfully_resolved_dns,
                                     int first_dns_error) = 0;
  };

  // Does not take ownership of |delegate|.
  // |try_ssltcp_first| indicates that SSLTCP is tried before
  // XMPP. Used by tests.
  // |server_list| is the list of connections to attempt in priority order.
  // |server_count| is the number of items in the server list.
  XmppConnectionGenerator(
      Delegate* delegate,
      net::HostResolver* host_resolver,
      const ConnectionOptions* options,
      bool try_ssltcp_first,
      const ServerList& servers);
  ~XmppConnectionGenerator();

  // Only call this once. Create a new XmppConnectionGenerator and delete the
  // current one if the process needs to start again.
  void StartGenerating();

  void UseNextConnection();

  // TODO(sanjeevr): Rip out the DNS resolution code eventually.
  void SetShouldResolveDNS(bool should_resolve_dns) {
    should_resolve_dns_ = should_resolve_dns;
  }

 private:
  void OnServerDNSResolved(int status);
  void HandleServerDNSResolved(int status);
  void GenerateSettingsForIPList(const std::vector<uint32>& ip_list);

  Delegate* delegate_;
  net::SingleRequestHostResolver host_resolver_;
  net::AddressList address_list_;
  net::BoundNetLog bound_net_log_;
  scoped_ptr<ConnectionSettingsList> settings_list_;
  int settings_index_;  // The setting that is currently being used.
  const ServerList servers_;
  ServerList::const_iterator current_server_;
  bool try_ssltcp_first_;  // Used when sync tests are run on chromium builders.
  bool successfully_resolved_dns_;
  int first_dns_error_;
  bool should_resolve_dns_;
  const ConnectionOptions* options_;

  DISALLOW_COPY_AND_ASSIGN(XmppConnectionGenerator);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
