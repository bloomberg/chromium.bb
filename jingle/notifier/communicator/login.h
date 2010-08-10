// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <string>

#include "base/time.h"
#include "base/timer.h"
#include "jingle/notifier/base/sigslotrepeater.h"
#include "jingle/notifier/communicator/login_connection_state.h"
#include "net/base/network_change_notifier.h"
#include "talk/base/proxyinfo.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class XmppClient;
class XmppEngine;
class XmppClientSettings;
}  // namespace buzz

namespace net {
class HostResolver;
}  // namespace net

namespace talk_base {
class FirewallManager;
struct ProxyInfo;
class TaskParent;
}  // namespace talk_base

namespace notifier {

class ConnectionOptions;
class LoginFailure;
class LoginSettings;
struct ServerInformation;
class SingleLoginAttempt;

// Does the login, keeps it alive (with refreshing cookies and reattempting
// login when disconnected), figures out what actions to take on the various
// errors that may occur.
class Login : public net::NetworkChangeNotifier::Observer,
              public sigslot::has_slots<> {
 public:
  // firewall may be NULL.
  Login(talk_base::TaskParent* parent,
        bool use_chrome_async_socket,
        const buzz::XmppClientSettings& user_settings,
        const ConnectionOptions& options,
        std::string lang,
        net::HostResolver* host_resolver,
        ServerInformation* server_list,
        int server_count,
        talk_base::FirewallManager* firewall,
        bool try_ssltcp_first,
        bool proxy_only);
  virtual ~Login();

  void StartConnection();

  buzz::XmppClient* xmpp_client();

  // net::NetworkChangeNotifier::Observer implementation.
  virtual void OnIPAddressChanged();

  sigslot::signal1<LoginConnectionState> SignalClientStateChange;

  sigslot::signal1<const LoginFailure&> SignalLoginFailure;
  sigslot::repeater2<const char*, int> SignalLogInput;
  sigslot::repeater2<const char*, int> SignalLogOutput;

 private:
  void OnLoginFailure(const LoginFailure& failure);
  void OnLogoff();
  void OnRedirect(const std::string& redirect_server, int redirect_port);
  void OnClientStateChange(buzz::XmppEngine::State state);

  void ChangeState(LoginConnectionState new_state);

  // Abort any existing connection.
  void Disconnect();

  // Stops any existing reconnect timer and sets an initial reconnect
  // interval.
  void ResetReconnectState();

  // Tries to reconnect in some point in the future.  If called
  // repeatedly, will wait longer and longer until reconnecting.
  void TryReconnect();

  // The actual function (called by |reconnect_timer_|) that does the
  // reconnection.
  void DoReconnect();

  talk_base::TaskParent* parent_;
  bool use_chrome_async_socket_;
  scoped_ptr<LoginSettings> login_settings_;
  LoginConnectionState state_;
  SingleLoginAttempt* single_attempt_;

  // reconnection state.
  base::TimeDelta reconnect_interval_;
  base::OneShotTimer<Login> reconnect_timer_;

  // server redirect information
  base::Time redirect_time_;
  std::string redirect_server_;
  int redirect_port_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
