// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_

#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/socketaddress.h"

namespace talk_base {
class AutoDetectProxy;
struct ProxyInfo;
class SignalThread;
class Task;
}

namespace notifier {

class AsyncDNSLookup;
class ConnectionOptions;
class ConnectionSettings;
class ConnectionSettingsList;

struct ServerInformation {
  talk_base::SocketAddress server;
  bool special_port_magic;
};

// Resolves dns names and iterates through the various ip address and transport
// combinations.
class XmppConnectionGenerator : public sigslot::has_slots<> {
 public:
  // parent is the parent for any tasks needed during this operation.
  // proxy_only indicates if true connections are only attempted using the
  // proxy.
  // server_list is the list of connections to attempt in priority order.
  // server_count is the number of items in the server list.
  XmppConnectionGenerator(talk_base::Task* parent,
                          const ConnectionOptions* options,
                          bool proxy_only,
                          const ServerInformation* server_list,
                          int server_count);
  ~XmppConnectionGenerator();

  // Only call this once. Create a new XmppConnectionGenerator and delete the
  // current one if the process needs to start again.
  void StartGenerating();

  void UseNextConnection();
  void UseCurrentConnection();

  const talk_base::ProxyInfo& proxy() const;

  sigslot::signal1<const ConnectionSettings&> SignalNewSettings;

  // SignalExhaustedSettings(bool successfully_resolved_dns,
  //                         int first_dns_error);
  sigslot::signal2<bool, int> SignalExhaustedSettings;

 private:
  void OnProxyDetect(talk_base::AutoDetectProxy* proxy_detect);
  void OnServerDNSResolved(AsyncDNSLookup* dns_lookup);
  void HandleExhaustedConnections();

  talk_base::scoped_ptr<ConnectionSettingsList> settings_list_;
  int settings_index_;  // The setting that is currently being used.
  talk_base::scoped_array<ServerInformation> server_list_;
  int server_count_;
  int server_index_;  // The server that is current being used.
  bool proxy_only_;
  bool successfully_resolved_dns_;
  int first_dns_error_;
  const ConnectionOptions* options_;

  talk_base::Task* parent_;
  DISALLOW_COPY_AND_ASSIGN(XmppConnectionGenerator);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_XMPP_CONNECTION_GENERATOR_H_
