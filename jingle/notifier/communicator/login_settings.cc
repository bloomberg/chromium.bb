// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "jingle/notifier/communicator/login_settings.h"

#include "base/logging.h"
#include "jingle/notifier/base/server_information.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "net/base/cert_verifier.h"
#include "talk/base/common.h"
#include "talk/base/socketaddress.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

LoginSettings::LoginSettings(const buzz::XmppClientSettings& user_settings,
                             const ConnectionOptions& options,
                             const scoped_refptr<net::URLRequestContextGetter>&
                                 request_context_getter,
                             const ServerList& servers,
                             bool try_ssltcp_first,
                             const std::string& auth_mechanism)
    :  try_ssltcp_first_(try_ssltcp_first),
       request_context_getter_(request_context_getter),
       servers_(servers),
       user_settings_(new buzz::XmppClientSettings(user_settings)),
       connection_options_(new ConnectionOptions(options)),
       auth_mechanism_(auth_mechanism) {
  DCHECK_GT(servers_.size(), 0u);
}

// Defined so that the destructors are executed here (and the corresponding
// classes don't need to be included in the header file).
LoginSettings::~LoginSettings() {
}

void LoginSettings::set_server_override(
    const net::HostPortPair& server) {
  server_override_.reset(
      new ServerInformation(server, servers_[0].special_port_magic));
}

void LoginSettings::clear_server_override() {
  server_override_.reset();
}

}  // namespace notifier
