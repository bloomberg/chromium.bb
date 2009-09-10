// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_H_
#include <string>

#include "chrome/browser/sync/notifier/base/time.h"
#include "chrome/browser/sync/notifier/gaia_auth/sigslotrepeater.h"
#include "talk/base/proxyinfo.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class CaptchaChallenge;
class XmppClient;
class XmppEngine;
class XmppClientSettings;
}  // namespace buzz

namespace talk_base {
class FirewallManager;
struct ProxyInfo;
class Task;
}  // namespace talk_base

namespace notifier {
class AutoReconnect;
class ConnectionOptions;
class LoginFailure;
class LoginSettings;
class NetworkStatusDetectorTask;
struct ServerInformation;
class SingleLoginAttempt;
class Timer;

// Does the login, keeps it alive (with refreshing cookies
// and reattempting login when disconnected), figures out
// what actions to take on the various errors that may occur.
class Login : public sigslot::has_slots<> {
 public:
  // network_status and firewall may be NULL
  Login(talk_base::Task* parent,
        const buzz::XmppClientSettings& user_settings,
        const ConnectionOptions& options,
        std::string lang,
        ServerInformation* server_list,
        int server_count,
        NetworkStatusDetectorTask* network_status,
        talk_base::FirewallManager* firewall,
        bool no_gaia_auth,
        bool proxy_only,
        bool previous_login_successful);
  ~Login();

  enum ConnectionState {
    STATE_CLOSED,
    // Same as the closed state but indicates that a countdown is happening
    // for auto-retrying the connection.
    STATE_RETRYING,
    STATE_OPENING,
    STATE_OPENED,
  };

  ConnectionState connection_state() const {
    return state_;
  }

  void StartConnection();
  void UseNextConnection();
  void UseCurrentConnection();
  buzz::XmppClient* xmpp_client();

  // Start the auto-reconnect.  It may not do the auto-reconnect
  // if auto-reconnect is turned off.
  void DoAutoReconnect();

  const LoginSettings& login_settings() const {
    return *(login_settings_.get());
  }

  // Returns the best guess at the host responsible for
  // the account (which we use to determine if it is
  // a dasher account or not).
  //
  // After login this may return a more accurate answer,
  // which accounts for open sign-up accounts.
  const std::string& google_host() const;

  // Analogous to google_host but for the user account.
  // ("fred" in "fred@gmail.com")
  const std::string& google_user() const;

  // Returns the proxy that is being used to connect (or
  // the default proxy information if all attempted
  // connections failed).
  //
  // Do not call until StartConnection has been called.
  const talk_base::ProxyInfo& proxy() const;

  int seconds_until_reconnect() const;

  // SignalClientStateChange(ConnectionState new_state);
  sigslot::signal1<ConnectionState> SignalClientStateChange;

  sigslot::signal1<const LoginFailure&> SignalLoginFailure;
  sigslot::repeater2<const char*, int> SignalLogInput;
  sigslot::repeater2<const char*, int> SignalLogOutput;
  sigslot::repeater1<bool> SignalIdleChange;

  // The creator should hook this up to a signal that indicates when the power
  // is being suspended.
  sigslot::repeater1<bool> SignalPowerSuspended;

 private:
  void OnRedirect(const std::string& redirect_server, int redirect_port);
  void OnUnexpectedDisconnect();
  void OnClientStateChange(buzz::XmppEngine::State state);
  void OnLoginFailure(const LoginFailure& failure);
  void OnLogoff();
  void OnAutoReconnectTimerChange();

  void HandleClientStateChange(ConnectionState new_state);
  void ResetUnexpectedDisconnect();

  void OnNetworkStateDetected(bool was_alive, bool is_alive);
  void OnDisconnectTimeout();

  scoped_ptr<LoginSettings> login_settings_;
  scoped_ptr<AutoReconnect> auto_reconnect_;
  SingleLoginAttempt* single_attempt_;
  bool successful_connection_;
  talk_base::Task* parent_;

  ConnectionState state_;

  // server redirect information
  time64 redirect_time_ns_;
  std::string redirect_server_;
  int redirect_port_;
  bool unexpected_disconnect_occurred_;
  Timer* reset_unexpected_timer_;
  std::string google_host_;
  std::string google_user_;
  talk_base::ProxyInfo proxy_info_;

  Timer* disconnect_timer_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};
}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_H_
