// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "jingle/notifier/communicator/single_login_attempt.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "jingle/notifier/communicator/const_communicator.h"
#include "jingle/notifier/communicator/gaia_token_pre_xmpp_auth.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/constants.h"

namespace net {
class NetLog;
}  // namespace net

namespace notifier {

SingleLoginAttempt::SingleLoginAttempt(LoginSettings* login_settings,
                                       Delegate* delegate)
    : login_settings_(login_settings),
      delegate_(delegate),
      connection_generator_(
          ALLOW_THIS_IN_INITIALIZER_LIST(this),
          login_settings_->host_resolver(),
          &login_settings_->connection_options(),
          login_settings_->try_ssltcp_first(),
          login_settings_->server_list(),
          login_settings_->server_count()) {
  connection_generator_.StartGenerating();
}

SingleLoginAttempt::~SingleLoginAttempt() {}

void SingleLoginAttempt::OnConnect(base::WeakPtr<talk_base::Task> base_task) {
  delegate_->OnConnect(base_task);
}

void SingleLoginAttempt::OnError(buzz::XmppEngine::Error error, int subcode,
                                 const buzz::XmlElement* stream_error) {
  VLOG(1) << "Error: " << error << ", subcode: " << subcode;
  if (stream_error) {
    DCHECK_EQ(error, buzz::XmppEngine::ERROR_STREAM);
    VLOG(1) << "Stream error: " << XmlElementToString(*stream_error);
  }

  // Check for redirection.
  if (stream_error) {
    const buzz::XmlElement* other =
        stream_error->FirstNamed(buzz::QN_XSTREAM_SEE_OTHER_HOST);
    if (other) {
      const buzz::XmlElement* text =
          stream_error->FirstNamed(buzz::QN_XSTREAM_TEXT);
      if (text) {
        // Yep, its a "stream:error" with "see-other-host" text,
        // let's parse out the server:port, and then reconnect
        // with that.
        const std::string& redirect = text->BodyText();
        size_t colon = redirect.find(":");
        int redirect_port = kDefaultXmppPort;
        std::string redirect_server;
        if (colon == std::string::npos) {
          redirect_server = redirect;
        } else {
          redirect_server = redirect.substr(0, colon);
          const std::string& port_text = redirect.substr(colon + 1);
          std::istringstream ist(port_text);
          ist >> redirect_port;
        }
        // We never allow a redirect to port 0.
        if (redirect_port == 0) {
          redirect_port = kDefaultXmppPort;
        }
        delegate_->OnRedirect(redirect_server, redirect_port);
        // May be deleted at this point.
        return;
      }
    }
  }

  // Iterate to the next possible connection (still trying to connect).
  connection_generator_.UseNextConnection();
}

void SingleLoginAttempt::OnNewSettings(
    const ConnectionSettings& connection_settings) {
  // TODO(akalin): Resolve any unresolved IPs, possibly through a
  // proxy, instead of skipping them.
  if (connection_settings.server().IsUnresolvedIP()) {
    connection_generator_.UseNextConnection();
    return;
  }

  buzz::XmppClientSettings client_settings =
      login_settings_->user_settings();
  // Fill in the rest of the client settings.
  connection_settings.FillXmppClientSettings(&client_settings);

  buzz::Jid jid(client_settings.user(), client_settings.host(),
                buzz::STR_EMPTY);
  buzz::PreXmppAuth* pre_xmpp_auth =
      new GaiaTokenPreXmppAuth(
          jid.Str(), client_settings.auth_cookie(),
          client_settings.token_service(),
          login_settings_->auth_mechanism());
  xmpp_connection_.reset(
      new XmppConnection(client_settings, login_settings_->cert_verifier(),
                         this, pre_xmpp_auth));
}

void SingleLoginAttempt::OnExhaustedSettings(
    bool successfully_resolved_dns,
    int first_dns_error) {
  if (!successfully_resolved_dns)
    VLOG(1) << "Could not resolve DNS: " << first_dns_error;
  VLOG(1) << "Could not connect to any XMPP server";
  delegate_->OnNeedReconnect();
}

}  // namespace notifier
