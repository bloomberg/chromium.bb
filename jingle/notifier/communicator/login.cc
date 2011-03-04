// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/login.h"

#include <string>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/login_settings.h"
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

Login::Login(Delegate* delegate,
             const buzz::XmppClientSettings& user_settings,
             const ConnectionOptions& options,
             net::HostResolver* host_resolver,
             net::CertVerifier* cert_verifier,
             ServerInformation* server_list,
             int server_count,
             bool try_ssltcp_first)
    : delegate_(delegate),
      login_settings_(new LoginSettings(user_settings,
                                        options,
                                        host_resolver,
                                        cert_verifier,
                                        server_list,
                                        server_count,
                                        try_ssltcp_first)),
      redirect_port_(0) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  ResetReconnectState();
}

Login::~Login() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
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

  VLOG(1) << "Starting connection...";

  single_attempt_.reset(new SingleLoginAttempt(login_settings_.get(), this));
}

void Login::OnConnect(base::WeakPtr<talk_base::Task> base_task) {
  ResetReconnectState();
  delegate_->OnConnect(base_task);
}

void Login::OnNeedReconnect() {
  TryReconnect();
}

void Login::OnRedirect(const std::string& redirect_server, int redirect_port) {
  DCHECK_NE(redirect_port_, 0);

  redirect_time_ = base::Time::Now();
  redirect_server_ = redirect_server;
  redirect_port_ = redirect_port;

  // Drop the current connection, and start the login process again.
  StartConnection();
}

void Login::OnIPAddressChanged() {
  VLOG(1) << "Detected IP address change";
  // Reconnect in 1 to 9 seconds (vary the time a little to try to
  // avoid spikey behavior on network hiccups).
  reconnect_interval_ = base::TimeDelta::FromSeconds(base::RandInt(1, 9));
  TryReconnect();
}

void Login::ResetReconnectState() {
  reconnect_interval_ =
      base::TimeDelta::FromSeconds(base::RandInt(5, 25));
  reconnect_timer_.Stop();
}

void Login::TryReconnect() {
  DCHECK_GT(reconnect_interval_.InSeconds(), 0);
  single_attempt_.reset();
  reconnect_timer_.Stop();
  VLOG(1) << "Reconnecting in "
          << reconnect_interval_.InSeconds() << " seconds";
  reconnect_timer_.Start(
      reconnect_interval_, this, &Login::DoReconnect);
  delegate_->OnDisconnect();
}

void Login::DoReconnect() {
  // Double reconnect time up to 30 minutes.
  const base::TimeDelta kMaxReconnectInterval =
      base::TimeDelta::FromMinutes(30);
  reconnect_interval_ *= 2;
  if (reconnect_interval_ > kMaxReconnectInterval)
    reconnect_interval_ = kMaxReconnectInterval;
  VLOG(1) << "Reconnecting...";
  StartConnection();
}

}  // namespace notifier
