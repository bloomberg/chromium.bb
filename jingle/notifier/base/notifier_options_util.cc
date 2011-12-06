// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/notifier_options_util.h"

#include "base/logging.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/notifier_options.h"
#include "talk/xmpp/jid.h"

namespace notifier {

buzz::XmppClientSettings MakeXmppClientSettings(
    const NotifierOptions& notifier_options,
    const std::string& email, const std::string& token,
    const std::string& token_service) {
  buzz::Jid jid = buzz::Jid(email);
  DCHECK(!jid.node().empty());
  DCHECK(jid.IsValid());

  buzz::XmppClientSettings xmpp_client_settings;
  xmpp_client_settings.set_user(jid.node());
  xmpp_client_settings.set_resource("chrome-sync");
  xmpp_client_settings.set_host(jid.domain());
  xmpp_client_settings.set_use_tls(buzz::TLS_ENABLED);
  xmpp_client_settings.set_auth_cookie(
      notifier_options.invalidate_xmpp_login ?
      token + "bogus" : token);
  xmpp_client_settings.set_token_service(token_service);
  if (notifier_options.allow_insecure_connection) {
    xmpp_client_settings.set_allow_plain(true);
    xmpp_client_settings.set_use_tls(buzz::TLS_DISABLED);
  }
  return xmpp_client_settings;
}

ServerList GetServerList(
    const NotifierOptions& notifier_options) {
  ServerList servers;
  // Override the default servers with a test notification server if one was
  // provided.
  if (!notifier_options.xmpp_host_port.host().empty()) {
    servers.push_back(
        ServerInformation(notifier_options.xmpp_host_port, false));
  } else {
    // The default servers know how to serve over port 443 (that's the magic).
    servers.push_back(
        ServerInformation(
            net::HostPortPair("talk.google.com",
                              notifier::kDefaultXmppPort),
            true));
    servers.push_back(
        ServerInformation(
            net::HostPortPair("talkx.l.google.com",
                              notifier::kDefaultXmppPort),
            true));
  }
  return servers;
}

}  // namespace notifier
