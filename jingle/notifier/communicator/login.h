// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <string>

#include "base/time.h"
#include "base/timer.h"
#include "jingle/notifier/base/sigslotrepeater.h"
#include "jingle/notifier/communicator/auto_reconnect.h"
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
        bool proxy_only,
        bool previous_login_successful);
  virtual ~Login();

  LoginConnectionState connection_state() const {
    return state_;
  }

  void StartConnection();
  void UseNextConnection();
  void UseCurrentConnection();
  buzz::XmppClient* xmpp_client();

  // Start the auto-reconnect.  It may not do the auto-reconnect if
  // auto-reconnect is turned off.
  void DoAutoReconnect();

  const LoginSettings& login_settings() const {
    return *(login_settings_.get());
  }

  // Returns the best guess at the host responsible for the account (which we
  // use to determine if it is a dasher account or not).
  //
  // After login this may return a more accurate answer, which accounts for
  // open sign-up accounts.
  const std::string& google_host() const;

  // Analogous to google_host but for the user account ("fred" in
  // "fred@gmail.com").
  const std::string& google_user() const;

  // Returns the proxy that is being used to connect (or the default proxy
  // information if all attempted connections failed).
  //
  // Do not call until StartConnection has been called.
  const talk_base::ProxyInfo& proxy() const;

  virtual void OnIPAddressChanged();

  sigslot::signal1<LoginConnectionState> SignalClientStateChange;

  sigslot::signal1<const LoginFailure&> SignalLoginFailure;
  sigslot::repeater2<const char*, int> SignalLogInput;
  sigslot::repeater2<const char*, int> SignalLogOutput;
  sigslot::repeater1<bool> SignalIdleChange;

  // The creator should hook this up to a signal that indicates when the power
  // is being suspended.
  sigslot::repeater1<bool> SignalPowerSuspended;

 private:
  void CheckConnection();

  void OnRedirect(const std::string& redirect_server, int redirect_port);
  void OnUnexpectedDisconnect();
  void OnClientStateChange(buzz::XmppEngine::State state);
  void OnLoginFailure(const LoginFailure& failure);
  void OnLogoff();
  void OnAutoReconnectTimerChange();

  void HandleClientStateChange(LoginConnectionState new_state);
  void ResetUnexpectedDisconnect();

  void OnDisconnectTimeout();

  talk_base::TaskParent* parent_;
  bool use_chrome_async_socket_;
  scoped_ptr<LoginSettings> login_settings_;
  AutoReconnect auto_reconnect_;
  SingleLoginAttempt* single_attempt_;
  bool successful_connection_;

  LoginConnectionState state_;

  // server redirect information
  base::Time redirect_time_;
  std::string redirect_server_;
  int redirect_port_;
  bool unexpected_disconnect_occurred_;
  base::OneShotTimer<Login> reset_unexpected_timer_;
  std::string google_host_;
  std::string google_user_;
  talk_base::ProxyInfo proxy_info_;

  base::OneShotTimer<Login> disconnect_timer_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
