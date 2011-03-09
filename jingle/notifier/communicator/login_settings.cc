// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "jingle/notifier/communicator/login_settings.h"

#include "base/logging.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "net/base/cert_verifier.h"
#include "talk/base/common.h"
#include "talk/base/socketaddress.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

LoginSettings::LoginSettings(const buzz::XmppClientSettings& user_settings,
                             const ConnectionOptions& options,
                             net::HostResolver* host_resolver,
                             net::CertVerifier* cert_verifier,
                             ServerInformation* server_list,
                             int server_count,
                             bool try_ssltcp_first,
                             const std::string& auth_mechanism)
    :  try_ssltcp_first_(try_ssltcp_first),
       host_resolver_(host_resolver),
       cert_verifier_(cert_verifier),
       server_list_(new ServerInformation[server_count]),
       server_count_(server_count),
       user_settings_(new buzz::XmppClientSettings(user_settings)),
       connection_options_(new ConnectionOptions(options)),
       auth_mechanism_(auth_mechanism) {
  // Note: firewall may be NULL.
  DCHECK(server_list);
  DCHECK(host_resolver);
  DCHECK(cert_verifier);
  DCHECK_GT(server_count, 0);
  for (int i = 0; i < server_count_; ++i) {
    server_list_[i] = server_list[i];
  }
}

// Defined so that the destructors are executed here (and the corresponding
// classes don't need to be included in the header file).
LoginSettings::~LoginSettings() {
}

void LoginSettings::set_server_override(
    const net::HostPortPair& server) {
  server_override_.reset(new ServerInformation());
  server_override_->server = server;
  server_override_->special_port_magic = server_list_[0].special_port_magic;
}

void LoginSettings::clear_server_override() {
  server_override_.reset();
}

}  // namespace notifier
