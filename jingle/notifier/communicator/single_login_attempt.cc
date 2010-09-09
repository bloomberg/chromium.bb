// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "jingle/notifier/communicator/single_login_attempt.h"

#include "base/logging.h"
#include "jingle/notifier/base/chrome_async_socket.h"
#include "jingle/notifier/base/xmpp_client_socket_factory.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "jingle/notifier/communicator/const_communicator.h"
#include "jingle/notifier/communicator/gaia_token_pre_xmpp_auth.h"
#include "jingle/notifier/communicator/login_failure.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/constants.h"

namespace net {
class NetLog;
}  // namespace net

namespace notifier {

static void GetClientErrorInformation(
    buzz::XmppClient* client,
    buzz::XmppEngine::Error* error,
    int* subcode,
    buzz::XmlElement** stream_error) {
  DCHECK(client);
  DCHECK(error);
  DCHECK(subcode);
  DCHECK(stream_error);

  *error = client->GetError(subcode);

  *stream_error = NULL;
  if (*error == buzz::XmppEngine::ERROR_STREAM) {
    const buzz::XmlElement* error_element = client->GetStreamError();
    if (error_element) {
      *stream_error = new buzz::XmlElement(*error_element);
    }
  }
}

SingleLoginAttempt::SingleLoginAttempt(talk_base::TaskParent* parent,
                                       LoginSettings* login_settings)
    : talk_base::Task(parent),
      state_(buzz::XmppEngine::STATE_NONE),
      code_(buzz::XmppEngine::ERROR_NONE),
      subcode_(0),
      need_authentication_(false),
      certificate_expired_(false),
      login_settings_(login_settings),
      client_(NULL) {
  connection_generator_.reset(new XmppConnectionGenerator(
                                  this,
                                  login_settings_->host_resolver(),
                                  &login_settings_->connection_options(),
                                  login_settings_->try_ssltcp_first(),
                                  login_settings_->proxy_only(),
                                  login_settings_->server_list(),
                                  login_settings_->server_count()));

  connection_generator_->SignalExhaustedSettings.connect(
      this,
      &SingleLoginAttempt::OnAttemptedAllConnections);
  connection_generator_->SignalNewSettings.connect(
      this,
      &SingleLoginAttempt::DoLogin);
}

SingleLoginAttempt::~SingleLoginAttempt() {
  // If this assertion goes off, it means that "Stop()" didn't get called like
  // it should have been.
  DCHECK(!client_);
}

const talk_base::ProxyInfo& SingleLoginAttempt::proxy() const {
  DCHECK(connection_generator_.get());
  return connection_generator_->proxy();
}

int SingleLoginAttempt::ProcessStart() {
  DCHECK_EQ(GetState(), talk_base::Task::STATE_START);
  connection_generator_->StartGenerating();

  // After being started, this class is callback driven and does signaling from
  // those callbacks (with checks to see if it is done if it may be called back
  // from something that isn't a child task).
  return talk_base::Task::STATE_BLOCKED;
}

void SingleLoginAttempt::Stop() {
  ClearClient();
  talk_base::Task::Stop();

  // No more signals should happen after being stopped. This is needed because
  // some of these signals happen due to other components doing signaling which
  // may continue running even though this task is stopped.
  SignalUnexpectedDisconnect.disconnect_all();
  SignalRedirect.disconnect_all();
  SignalLoginFailure.disconnect_all();
  SignalNeedAutoReconnect.disconnect_all();
  SignalClientStateChange.disconnect_all();
}

void SingleLoginAttempt::OnAttemptedAllConnections(
    bool successfully_resolved_dns,
    int first_dns_error) {

  // Maybe we needed proxy authentication?
  if (need_authentication_) {
    LoginFailure failure(LoginFailure::PROXY_AUTHENTICATION_ERROR);
    SignalLoginFailure(failure);
    return;
  }

  if (certificate_expired_) {
    LoginFailure failure(LoginFailure::CERTIFICATE_EXPIRED_ERROR);
    SignalLoginFailure(failure);
    return;
  }

  if (!successfully_resolved_dns) {
    code_ = buzz::XmppEngine::ERROR_SOCKET;
    subcode_ = first_dns_error;
  }

  LOG(INFO) << "Connection failed with error " << code_;

  SignalNeedAutoReconnect();
}

void SingleLoginAttempt::UseNextConnection() {
  DCHECK(connection_generator_.get());
  ClearClient();
  connection_generator_->UseNextConnection();
}

void SingleLoginAttempt::UseCurrentConnection() {
  DCHECK(connection_generator_.get());
  ClearClient();
  connection_generator_->UseCurrentConnection();
}

void SingleLoginAttempt::DoLogin(
    const ConnectionSettings& connection_settings) {
  if (client_) {
    return;
  }
  // TODO(akalin): Resolve any unresolved IPs, possibly through a
  // proxy, instead of skipping them.
  if (connection_settings.server().IsUnresolvedIP()) {
    // Handler should attempt reconnect.
    SignalUnexpectedDisconnect();
    return;
  }

  buzz::XmppClientSettings client_settings;
  // Set the user settings portion.
  *static_cast<buzz::XmppClientSettings*>(&client_settings) =
      login_settings_->user_settings();
  // Fill in the rest of the client settings.
  connection_settings.FillXmppClientSettings(&client_settings);

  client_ = new buzz::XmppClient(this);
  SignalLogInput.repeat(client_->SignalLogInput);
  SignalLogOutput.repeat(client_->SignalLogOutput);

  // Listen for connection progress.
  client_->SignalStateChange.connect(this,
                                     &SingleLoginAttempt::OnClientStateChange);

  // Transition to "start".
  OnClientStateChange(buzz::XmppEngine::STATE_START);
  // Start connecting.
  client_->Connect(client_settings, login_settings_->lang(),
                   CreateSocket(client_settings),
                   CreatePreXmppAuth(client_settings));
  client_->Start();
}

void SingleLoginAttempt::OnAuthenticationError() {
  // We can check this flag later if all connection options fail.
  need_authentication_ = true;
}

void SingleLoginAttempt::OnCertificateExpired() {
  // We can check this flag later if all connection options fail.
  certificate_expired_ = true;
}

buzz::AsyncSocket* SingleLoginAttempt::CreateSocket(
    const buzz::XmppClientSettings& xcs) {
  bool use_fake_ssl_client_socket =
      (xcs.protocol() == cricket::PROTO_SSLTCP);
  net::ClientSocketFactory* const client_socket_factory =
      new XmppClientSocketFactory(
          net::ClientSocketFactory::GetDefaultFactory(),
          use_fake_ssl_client_socket);
  // The default SSLConfig is good enough for us for now.
  const net::SSLConfig ssl_config;
  // A read buffer of 64k ought to be sufficient.
  const size_t kReadBufSize = 64U * 1024U;
  // This number was taken from a similar number in
  // XmppSocketAdapter.
  const size_t kWriteBufSize = 64U * 1024U;
  // TODO(akalin): Use a real NetLog.
  net::NetLog* const net_log = NULL;
  return new ChromeAsyncSocket(
      client_socket_factory, ssl_config,
      kReadBufSize, kWriteBufSize, net_log);
}

buzz::PreXmppAuth* SingleLoginAttempt::CreatePreXmppAuth(
    const buzz::XmppClientSettings& xcs) {
  buzz::Jid jid(xcs.user(), xcs.host(), buzz::STR_EMPTY);
  return new GaiaTokenPreXmppAuth(
      jid.Str(), xcs.auth_cookie(), xcs.token_service());
}

void SingleLoginAttempt::OnClientStateChange(buzz::XmppEngine::State state) {
  if (state_ == state)
    return;

  buzz::XmppEngine::State previous_state = state_;
  state_ = state;

  switch (state) {
    case buzz::XmppEngine::STATE_NONE:
    case buzz::XmppEngine::STATE_START:
    case buzz::XmppEngine::STATE_OPENING:
    case buzz::XmppEngine::STATE_OPEN:
      // Do nothing.
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      OnClientStateChangeClosed(previous_state);
      break;
  }
  SignalClientStateChange(state);
  if (state_ == buzz::XmppEngine::STATE_CLOSED) {
    OnClientStateChange(buzz::XmppEngine::STATE_NONE);
  }
}

void SingleLoginAttempt::ClearClient() {
  if (client_ != NULL) {
    client_->Disconnect();

    // If this assertion goes off, it means that the disconnect didn't occur
    // properly.  See SingleLoginAttempt::OnClientStateChange,
    // case XmppEngine::STATE_CLOSED
    DCHECK(!client_);
  }
}

void SingleLoginAttempt::OnClientStateChangeClosed(
    buzz::XmppEngine::State previous_state) {
  buzz::XmppEngine::Error error = buzz::XmppEngine::ERROR_NONE;
  int error_subcode = 0;
  buzz::XmlElement* stream_error_ptr;
  GetClientErrorInformation(client_,
                            &error,
                            &error_subcode,
                            &stream_error_ptr);
  scoped_ptr<buzz::XmlElement> stream_error(stream_error_ptr);

  client_->SignalStateChange.disconnect(this);
  client_ = NULL;

  if (error == buzz::XmppEngine::ERROR_NONE) {
    SignalLogoff();
    return;
  } else if (previous_state == buzz::XmppEngine::STATE_OPEN) {
    // Handler should attempt reconnect.
    SignalUnexpectedDisconnect();
    return;
  } else {
    HandleConnectionError(error, error_subcode, stream_error.get());
  }
}

void SingleLoginAttempt::HandleConnectionPasswordError() {
  LOG(INFO) << "SingleLoginAttempt::HandleConnectionPasswordError";
  LoginFailure failure(LoginFailure::XMPP_ERROR, code_, subcode_);
  SignalLoginFailure(failure);
}

void SingleLoginAttempt::HandleConnectionError(
    buzz::XmppEngine::Error code,
    int subcode,
    const buzz::XmlElement* stream_error) {
  LOG(INFO) << "(" << code << ", " << subcode << ")";

  // Save off the error code information, so we can use it to tell the user
  // what went wrong if all else fails.
  code_ = code;
  subcode_ = subcode;
  if ((code_ == buzz::XmppEngine::ERROR_UNAUTHORIZED) ||
      (code_ == buzz::XmppEngine::ERROR_MISSING_USERNAME)) {
    // There was a problem with credentials (username/password).
    HandleConnectionPasswordError();
    return;
  }

  // Unexpected disconnect,
  // Unreachable host,
  // Or internal server binding error -
  // All these are temporary problems, so continue reconnecting.

  login_settings_->modifiable_user_settings()->set_resource("");

  // Look for stream::error server redirection stanza "see-other-host".
  if (stream_error) {
    const buzz::XmlElement* other =
        stream_error->FirstNamed(buzz::QN_XSTREAM_SEE_OTHER_HOST);
    if (other) {
      const buzz::XmlElement* text =
          stream_error->FirstNamed(buzz::QN_XSTREAM_TEXT);
      if (text) {
        // Yep, its a "stream:error" with "see-other-host" text, let's parse
        // out the server:port, and then reconnect with that.
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
        SignalRedirect(redirect_server, redirect_port);
        // May be deleted at this point.
        return;
      }
    }
  }

  DCHECK(connection_generator_.get());
  if (!connection_generator_.get()) {
    return;
  }

  // Iterate to the next possible connection (still trying to connect).
  UseNextConnection();
}

}  // namespace notifier
