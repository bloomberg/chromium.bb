// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "jingle/notifier/communicator/login.h"

#include "base/logging.h"
#include "base/time.h"
#include "jingle/notifier/communicator/auto_reconnect.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/communicator/product_info.h"
#include "jingle/notifier/communicator/single_login_attempt.h"
#include "net/base/host_port_pair.h"
#include "talk/base/common.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/taskrunner.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/asyncsocket.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

// Redirect valid for 5 minutes.
static const int kRedirectTimeoutMinutes = 5;

// Disconnect if network stays down for more than 10 seconds.
static const int kDisconnectionDelaySecs = 10;

Login::Login(talk_base::TaskParent* parent,
             bool use_chrome_async_socket,
             const buzz::XmppClientSettings& user_settings,
             const ConnectionOptions& options,
             std::string lang,
             net::HostResolver* host_resolver,
             ServerInformation* server_list,
             int server_count,
             talk_base::FirewallManager* firewall,
             bool proxy_only,
             bool previous_login_successful)
    : parent_(parent),
      use_chrome_async_socket_(use_chrome_async_socket),
      login_settings_(new LoginSettings(user_settings,
                                        options,
                                        lang,
                                        host_resolver,
                                        server_list,
                                        server_count,
                                        firewall,
                                        proxy_only)),
      single_attempt_(NULL),
      successful_connection_(previous_login_successful),
      state_(STATE_OPENING),
      redirect_port_(0),
      unexpected_disconnect_occurred_(false),
      google_host_(user_settings.host()),
      google_user_(user_settings.user()) {
  // Hook up all the signals and observers.
  net::NetworkChangeNotifier::AddObserver(this);
  auto_reconnect_.SignalStartConnection.connect(this,
                                                &Login::StartConnection);
  auto_reconnect_.SignalTimerStartStop.connect(
      this,
      &Login::OnAutoReconnectTimerChange);
  SignalClientStateChange.connect(&auto_reconnect_,
                                  &AutoReconnect::OnClientStateChange);
  SignalIdleChange.connect(&auto_reconnect_,
                           &AutoReconnect::set_idle);
  SignalPowerSuspended.connect(&auto_reconnect_,
                               &AutoReconnect::OnPowerSuspend);

  // Then check the initial state of the connection.
  CheckConnection();
}

// Defined so that the destructors are executed here (and the corresponding
// classes don't need to be included in the header file).
Login::~Login() {
  if (single_attempt_) {
    single_attempt_->Abort();
    single_attempt_ = NULL;
  }
  net::NetworkChangeNotifier::RemoveObserver(this);
}

void Login::StartConnection() {
  // If there is a server redirect, use it.
  if (base::Time::Now() <
      (redirect_time_ +
       base::TimeDelta::FromMinutes(kRedirectTimeoutMinutes))) {
    // Override server/port with redirect values.
    DCHECK_NE(redirect_port_, 0);
    net::HostPortPair server_override(redirect_server_, redirect_port_);
    login_settings_->set_server_override(server_override);
  } else {
    login_settings_->clear_server_override();
  }

  if (single_attempt_) {
    single_attempt_->Abort();
    single_attempt_ = NULL;
  }
  single_attempt_ = new SingleLoginAttempt(parent_,
                                           login_settings_.get(),
                                           use_chrome_async_socket_,
                                           successful_connection_);

  // Do the signaling hook-ups.
  single_attempt_->SignalLoginFailure.connect(this, &Login::OnLoginFailure);
  single_attempt_->SignalRedirect.connect(this, &Login::OnRedirect);
  single_attempt_->SignalClientStateChange.connect(
      this,
      &Login::OnClientStateChange);
  single_attempt_->SignalUnexpectedDisconnect.connect(
      this,
      &Login::OnUnexpectedDisconnect);
  single_attempt_->SignalLogoff.connect(
      this,
      &Login::OnLogoff);
  single_attempt_->SignalNeedAutoReconnect.connect(
      this,
      &Login::DoAutoReconnect);
  SignalLogInput.repeat(single_attempt_->SignalLogInput);
  SignalLogOutput.repeat(single_attempt_->SignalLogOutput);

  single_attempt_->Start();
}

const std::string& Login::google_host() const {
  return google_host_;
}

const std::string& Login::google_user() const {
  return google_user_;
}

const talk_base::ProxyInfo& Login::proxy() const {
  return proxy_info_;
}

void Login::OnLoginFailure(const LoginFailure& failure) {
  auto_reconnect_.StopReconnectTimer();
  HandleClientStateChange(STATE_CLOSED);
  SignalLoginFailure(failure);
}

void Login::OnLogoff() {
  HandleClientStateChange(STATE_CLOSED);
}

void Login::OnClientStateChange(buzz::XmppEngine::State state) {
  LoginConnectionState new_state = STATE_CLOSED;

  switch (state) {
    case buzz::XmppEngine::STATE_NONE:
    case buzz::XmppEngine::STATE_CLOSED:
      // Ignore the closed state (because we may be trying the next dns entry).
      //
      // But we go to this state for other signals when there is no retry
      // happening.
      new_state = state_;
      break;

    case buzz::XmppEngine::STATE_START:
    case buzz::XmppEngine::STATE_OPENING:
      new_state = STATE_OPENING;
      break;

    case buzz::XmppEngine::STATE_OPEN:
      new_state = STATE_OPENED;
      break;

    default:
      DCHECK(false);
      break;
  }
  HandleClientStateChange(new_state);
}

void Login::HandleClientStateChange(LoginConnectionState new_state) {
  // Do we need to transition between the retrying and closed states?
  if (auto_reconnect_.is_retrying()) {
    if (new_state == STATE_CLOSED) {
      new_state = STATE_RETRYING;
    }
  } else {
    if (new_state == STATE_RETRYING) {
      new_state = STATE_CLOSED;
    }
  }

  if (new_state != state_) {
    state_ = new_state;
    reset_unexpected_timer_.Stop();

    if (state_ == STATE_OPENED) {
      successful_connection_ = true;

      google_host_ = single_attempt_->xmpp_client()->jid().domain();
      google_user_ = single_attempt_->xmpp_client()->jid().node();
      proxy_info_ = single_attempt_->proxy();

      reset_unexpected_timer_.Start(
          base::TimeDelta::FromSeconds(kResetReconnectInfoDelaySec),
          this, &Login::ResetUnexpectedDisconnect);
    }
    SignalClientStateChange(state_);
  }
}

void Login::OnAutoReconnectTimerChange() {
  if (!single_attempt_ || !single_attempt_->xmpp_client()) {
    HandleClientStateChange(STATE_CLOSED);
    return;
  }
  OnClientStateChange(single_attempt_->xmpp_client()->GetState());
}

buzz::XmppClient* Login::xmpp_client() {
  if (!single_attempt_) {
    return NULL;
  }
  return single_attempt_->xmpp_client();
}

void Login::UseNextConnection() {
  if (!single_attempt_) {
    // Just in case, there is an obscure case that causes this to get called
    // when there is no single_attempt_.
    return;
  }
  single_attempt_->UseNextConnection();
}

void Login::UseCurrentConnection() {
  if (!single_attempt_) {
    // Just in case, there is an obscure case that causes this to get called
    // when there is no single_attempt_.
    return;
  }
  single_attempt_->UseCurrentConnection();
}

void Login::OnRedirect(const std::string& redirect_server, int redirect_port) {
  DCHECK_NE(redirect_port_, 0);

  redirect_time_ = base::Time::Now();
  redirect_server_ = redirect_server;
  redirect_port_ = redirect_port;

  // Drop the current connection, and start the login process again.
  StartConnection();
}

void Login::OnUnexpectedDisconnect() {
  reset_unexpected_timer_.Stop();

  // Start the login process again.
  if (unexpected_disconnect_occurred_) {
    // If we already have received an unexpected disconnect recently, then our
    // account may have be jailed due to abuse, so we shouldn't make the
    // situation worse by trying really hard to reconnect. Instead, we'll do
    // the autoreconnect route, which has exponential back-off.
    DoAutoReconnect();
    return;
  }
  StartConnection();
  unexpected_disconnect_occurred_ = true;
}

void Login::ResetUnexpectedDisconnect() {
  unexpected_disconnect_occurred_ = false;
}

void Login::DoAutoReconnect() {
  bool allow_auto_reconnect =
      login_settings_->connection_options().auto_reconnect();
  // Start the reconnect time before aborting the connection to ensure that
  // AutoReconnect::is_retrying() is true, so that the Login doesn't
  // transition to the CLOSED state (which would cause the reconnection timer
  // to reset and not double).
  if (allow_auto_reconnect) {
    auto_reconnect_.StartReconnectTimer();
  }

  if (single_attempt_) {
    single_attempt_->Abort();
    single_attempt_ = NULL;
  }

  if (!allow_auto_reconnect) {
    HandleClientStateChange(STATE_CLOSED);
    return;
  }
}

void Login::OnIPAddressChanged() {
  LOG(INFO) << "IP address change detected";
  CheckConnection();
}

void Login::CheckConnection() {
  // We don't check the connection if we're using ChromeAsyncSocket,
  // as this code requires a libjingle thread to be running.  This
  // code will go away in a future cleanup CL, anyway.
  if (!use_chrome_async_socket_) {
    LOG(INFO) << "Checking connection";
    talk_base::PhysicalSocketServer physical;
    scoped_ptr<talk_base::Socket> socket(physical.CreateSocket(SOCK_STREAM));
    bool alive =
        !socket->Connect(talk_base::SocketAddress("talk.google.com", 5222));
    LOG(INFO) << "Network is " << (alive ? "alive" : "not alive");
    if (alive) {
      // Our connection is up. If we have a disconnect timer going,
      // stop it so we don't disconnect.
      disconnect_timer_.Stop();
    } else {
      // Our network connection is down. Start the disconnect timer if
      // it's not already going.  Don't disconnect immediately to avoid
      // constant connection/disconnection due to flaky network
      // interfaces.
      if (!disconnect_timer_.IsRunning()) {
        disconnect_timer_.Start(
            base::TimeDelta::FromSeconds(kDisconnectionDelaySecs),
            this, &Login::OnDisconnectTimeout);
      }
    }
    auto_reconnect_.NetworkStateChanged(alive);
  }
}

void Login::OnDisconnectTimeout() {
  if (state_ != STATE_OPENED) {
    return;
  }

  if (single_attempt_) {
    single_attempt_->Abort();
    single_attempt_ = NULL;
  }

  StartConnection();
}

}  // namespace notifier
