// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
#include <string>

#include "jingle/notifier/base/server_information.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"

namespace buzz {
class XmppClientSettings;
}

namespace net {
class CertVerifier;
class HostPortPair;
class HostResolver;
}

namespace talk_base {
class SocketAddress;
}

namespace notifier {
class ConnectionOptions;

class LoginSettings {
 public:
  LoginSettings(const buzz::XmppClientSettings& user_settings,
                const ConnectionOptions& options,
                net::HostResolver* host_resolver,
                net::CertVerifier* cert_verifier,
                const ServerList& servers,
                bool try_ssltcp_first,
                const std::string& auth_mechanism);

  ~LoginSettings();

  bool try_ssltcp_first() const {
    return try_ssltcp_first_;
  }

  net::HostResolver* host_resolver() {
    return host_resolver_;
  }

  net::CertVerifier* cert_verifier() {
    return cert_verifier_;
  }

  ServerList servers() const {
    return
        server_override_.get() ? ServerList(1, *server_override_) : servers_;
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

  std::string auth_mechanism() const {
    return auth_mechanism_;
  }

 private:
  bool try_ssltcp_first_;

  net::HostResolver* const host_resolver_;
  net::CertVerifier* const cert_verifier_;
  const ServerList servers_;
  // Used to handle redirects
  scoped_ptr<ServerInformation> server_override_;

  scoped_ptr<buzz::XmppClientSettings> user_settings_;
  scoped_ptr<ConnectionOptions> connection_options_;
  std::string auth_mechanism_;

  DISALLOW_COPY_AND_ASSIGN(LoginSettings);
};
}  // namespace notifier
#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_SETTINGS_H_
