// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
#include <string>

#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "talk/base/scoped_ptr.h"

namespace buzz {
class XmppClientSettings;
}

namespace net {
class HostPortPair;
class HostResolver;
}

namespace talk_base {
class SocketAddress;
}

namespace notifier {
class ConnectionOptions;
struct ServerInformation;

class LoginSettings {
 public:
  LoginSettings(const buzz::XmppClientSettings& user_settings,
                const ConnectionOptions& options,
                net::HostResolver* host_resolver,
                ServerInformation* server_list,
                int server_count,
                bool try_ssltcp_first);

  ~LoginSettings();

  bool try_ssltcp_first() const {
    return try_ssltcp_first_;
  }

  net::HostResolver* host_resolver() {
    return host_resolver_;
  }

  const ServerInformation* server_list() const {
    return server_override_.get() ? server_override_.get() : server_list_.get();
  }

  int server_count() const {
    return server_override_.get() ? 1 : server_count_;
  }

  const buzz::XmppClientSettings& user_settings() const {
    return *user_settings_.get();
  }

  buzz::XmppClientSettings* modifiable_user_settings() {
    return user_settings_.get();
  }

  const ConnectionOptions& connection_options() const {
    return *connection_options_.get();
  }

  void set_server_override(const net::HostPortPair& server);
  void clear_server_override();

 private:
  bool try_ssltcp_first_;

  net::HostResolver* host_resolver_;
  talk_base::scoped_array<ServerInformation> server_list_;
  int server_count_;
  // Used to handle redirects
  scoped_ptr<ServerInformation> server_override_;

  scoped_ptr<buzz::XmppClientSettings> user_settings_;
  scoped_ptr<ConnectionOptions> connection_options_;
  DISALLOW_COPY_AND_ASSIGN(LoginSettings);
};
}  // namespace notifier
#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
