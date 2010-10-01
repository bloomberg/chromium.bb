// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_

#include <vector>

#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_log.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"

namespace talk_base {
struct ProxyInfo;
class SignalThread;
}

namespace notifier {

class ConnectionOptions;
class ConnectionSettings;
class ConnectionSettingsList;

struct ServerInformation {
  net::HostPortPair server;
  bool special_port_magic;
};

// Resolves dns names and iterates through the various ip address and transport
// combinations.
class XmppConnectionGenerator : public sigslot::has_slots<> {
 public:
  // try_ssltcp_first indicates that SSLTCP is tried before XMPP. Used by tests.
  // server_list is the list of connections to attempt in priority order.
  // server_count is the number of items in the server list.
  XmppConnectionGenerator(
      net::HostResolver* host_resolver,
      const ConnectionOptions* options,
      bool try_ssltcp_first,
      const ServerInformation* server_list,
      int server_count);
  ~XmppConnectionGenerator();

  // Only call this once. Create a new XmppConnectionGenerator and delete the
  // current one if the process needs to start again.
  void StartGenerating();

  void UseNextConnection();
  void UseCurrentConnection();

  sigslot::signal1<const ConnectionSettings&> SignalNewSettings;

  // SignalExhaustedSettings(bool successfully_resolved_dns,
  //                         int first_dns_error);
  sigslot::signal2<bool, int> SignalExhaustedSettings;

 private:
  void OnServerDNSResolved(int status);
  void HandleServerDNSResolved(int status);
  void HandleExhaustedConnections();

  net::SingleRequestHostResolver host_resolver_;
  scoped_ptr<net::CompletionCallback> resolve_callback_;
  net::AddressList address_list_;
  net::BoundNetLog bound_net_log_;
  talk_base::scoped_ptr<ConnectionSettingsList> settings_list_;
  int settings_index_;  // The setting that is currently being used.
  talk_base::scoped_array<ServerInformation> server_list_;
  int server_count_;
  int server_index_;  // The server that is current being used.
  bool try_ssltcp_first_;  // Used when sync tests are run on chromium builders.
  bool successfully_resolved_dns_;
  int first_dns_error_;
  const ConnectionOptions* options_;

  DISALLOW_COPY_AND_ASSIGN(XmppConnectionGenerator);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
