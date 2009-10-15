// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/sync/notifier/communicator/login.h"

#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"
#include "chrome/browser/sync/notifier/base/time.h"
#include "chrome/browser/sync/notifier/base/timer.h"
#include "chrome/browser/sync/notifier/communicator/auto_reconnect.h"
#include "chrome/browser/sync/notifier/communicator/connection_options.h"
#include "chrome/browser/sync/notifier/communicator/login_settings.h"
#include "chrome/browser/sync/notifier/communicator/product_info.h"
#include "chrome/browser/sync/notifier/communicator/single_login_attempt.h"
#include "talk/base/common.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/logging.h"
#include "talk/base/taskrunner.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/asyncsocket.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

// Redirect valid for 5 minutes.
static const time64 kRedirectTimeoutNs = 5 * kMinsTo100ns;

// Disconnect if network stays down for more than 10 seconds.
static const int kDisconnectionDelaySecs = 10;

Login::Login(talk_base::Task* parent,
             const buzz::XmppClientSettings& user_settings,
             const ConnectionOptions& options,
             std::string lang,
             ServerInformation* server_list,
             int server_count,
             NetworkStatusDetectorTask* network_status,
             talk_base::FirewallManager* firewall,
             bool no_gaia_auth,
             bool proxy_only,
             bool previous_login_successful)
    : login_settings_(new LoginSettings(user_settings,
                                        options,
                                        lang,
                                        server_list,
                                        server_count,
                                        firewall,
                                        no_gaia_auth,
                                        proxy_only)),
      single_attempt_(NULL),
      successful_connection_(previous_login_successful),
      parent_(parent),
      state_(STATE_OPENING),
      redirect_time_ns_(0),
      redirect_port_(0),
      unexpected_disconnect_occurred_(false),
      reset_unexpected_timer_(NULL),
      google_host_(user_settings.host()),
      google_user_(user_settings.user()),
      disconnect_timer_(NULL) {
  if (!network_status) {
    network_status = NetworkStatusDetectorTask::Create(parent_);
    if (network_status) {
      // On linux we don't have an implementation of NetworkStatusDetectorTask.
      network_status->Start();
    }
  }

  if (network_status) {
    network_status->SignalNetworkStateDetected.connect(
        this, &Login::OnNetworkStateDetected);
    auto_reconnect_.reset(new AutoReconnect(parent_, network_status));
    auto_reconnect_->SignalStartConnection.connect(this,
                                                   &Login::StartConnection);
    auto_reconnect_->SignalTimerStartStop.connect(
        this,
        &Login::OnAutoReconnectTimerChange);
    SignalClientStateChange.connect(auto_reconnect_.get(),
                                    &AutoReconnect::OnClientStateChange);
    SignalIdleChange.connect(auto_reconnect_.get(),
                             &AutoReconnect::set_idle);
    SignalPowerSuspended.connect(auto_reconnect_.get(),
                                 &AutoReconnect::OnPowerSuspend);
  }
}

// Defined so that the destructors are executed here (and the corresponding
// classes don't need to be included in the header file).
Login::~Login() {
  if (single_attempt_) {
    single_attempt_->Abort();
    single_attempt_ = NULL;
  }
}

void Login::StartConnection() {
  // If there is a server redirect, use it.
  if (GetCurrent100NSTime() < redirect_time_ns_ + kRedirectTimeoutNs) {
    // Override server/port with redirect values.
    talk_base::SocketAddress server_override;
    server_override.SetIP(redirect_server_, false);
    ASSERT(redirect_port_ != 0);
    server_override.SetPort(redirect_port_);
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
                                           successful_connection_);

  // Do the signaling hook-ups.
  single_attempt_->SignalLoginFailure.connect(this, &Login::OnLoginFailure);
  single_attempt_->SignalRedirect.connect(this, &Login::OnRedirect);
  single_attempt_->SignalClientStateChange.connect(
      this,
      &Login::OnClientStateChange) ;
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
  auto_reconnect_->StopReconnectTimer();
  HandleClientStateChange(STATE_CLOSED);
  SignalLoginFailure(failure);
}

void Login::OnLogoff() {
  HandleClientStateChange(STATE_CLOSED);
}

void Login::OnClientStateChange(buzz::XmppEngine::State state) {
  ConnectionState new_state = STATE_CLOSED;

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
      ASSERT(false);
      break;
  }
  HandleClientStateChange(new_state);
}

void Login::HandleClientStateChange(ConnectionState new_state) {
  // Do we need to transition between the retrying and closed states?
  if (auto_reconnect_.get() && auto_reconnect_->is_retrying()) {
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
    if (reset_unexpected_timer_) {
      reset_unexpected_timer_->Abort();
      reset_unexpected_timer_ = NULL;
    }

    if (state_ == STATE_OPENED) {
      successful_connection_ = true;

      google_host_ = single_attempt_->xmpp_client()->jid().domain();
      google_user_ = single_attempt_->xmpp_client()->jid().node();
      proxy_info_ = single_attempt_->proxy();

      reset_unexpected_timer_ = new Timer(parent_,
                                          kResetReconnectInfoDelaySec,
                                          false);  // Repeat.
      reset_unexpected_timer_->SignalTimeout.connect(
          this,
          &Login::ResetUnexpectedDisconnect);
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

int Login::seconds_until_reconnect() const {
  return auto_reconnect_->seconds_until();
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
  ASSERT(redirect_port_ != 0);

  redirect_time_ns_ = GetCurrent100NSTime();
  redirect_server_ = redirect_server;
  redirect_port_ = redirect_port;

  // Drop the current connection, and start the login process again.
  StartConnection();
}

void Login::OnUnexpectedDisconnect() {
  if (reset_unexpected_timer_) {
    reset_unexpected_timer_->Abort();
    reset_unexpected_timer_ = NULL;
  }

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
  reset_unexpected_timer_ = NULL;
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
    auto_reconnect_->StartReconnectTimer();
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

void Login::OnNetworkStateDetected(bool was_alive, bool is_alive) {
  if (was_alive && !is_alive) {
    // Our network connection just went down. Setup a timer to disconnect.
    // Don't disconnect immediately to avoid constant
    // connection/disconnection due to flaky network interfaces.
    ASSERT(disconnect_timer_ == NULL);
    disconnect_timer_ = new Timer(parent_, kDisconnectionDelaySecs, false);
    disconnect_timer_->SignalTimeout.connect(this,
                                             &Login::OnDisconnectTimeout);
  } else if (!was_alive && is_alive) {
    // Our connection has come back up. If we have a disconnect timer going,
    // abort it so we don't disconnect.
    if (disconnect_timer_) {
      disconnect_timer_->Abort();
      // It will free itself.
      disconnect_timer_ = NULL;
    }
  }
}

void Login::OnDisconnectTimeout() {
  disconnect_timer_ = NULL;

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
