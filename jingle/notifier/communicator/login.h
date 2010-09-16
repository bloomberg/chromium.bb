// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "net/base/network_change_notifier.h"
#include "talk/base/proxyinfo.h"
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
class Task;
class TaskParent;
}  // namespace talk_base

namespace notifier {

class ConnectionOptions;
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
  Login(const buzz::XmppClientSettings& user_settings,
        const ConnectionOptions& options,
        net::HostResolver* host_resolver,
        ServerInformation* server_list,
        int server_count,
        bool try_ssltcp_first);
  virtual ~Login();

  void StartConnection();

  // net::NetworkChangeNotifier::Observer implementation.
  virtual void OnIPAddressChanged();

  sigslot::signal1<base::WeakPtr<talk_base::Task> > SignalConnect;
  sigslot::signal0<> SignalDisconnect;

 private:
  void OnLogoff();
  void OnRedirect(const std::string& redirect_server, int redirect_port);
  void OnConnect(base::WeakPtr<talk_base::Task> parent);

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
  scoped_ptr<LoginSettings> login_settings_;
  scoped_ptr<SingleLoginAttempt> single_attempt_;

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
