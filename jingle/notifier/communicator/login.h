// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "jingle/notifier/base/server_information.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/communicator/single_login_attempt.h"
#include "net/base/network_change_notifier.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class XmppClient;
class XmppClientSettings;
class XmppTaskParentInterface;
}  // namespace buzz

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace notifier {

class LoginSettings;

// Workaround for MSVS 2005 bug that fails to handle inheritance from a nested
// class properly if it comes directly on a base class list.
typedef SingleLoginAttempt::Delegate SingleLoginAttemptDelegate;

// Does the login, keeps it alive (with refreshing cookies and reattempting
// login when disconnected), figures out what actions to take on the various
// errors that may occur.
class Login : public net::NetworkChangeNotifier::IPAddressObserver,
              public SingleLoginAttemptDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnConnect(
        base::WeakPtr<buzz::XmppTaskParentInterface> base_task) = 0;
    virtual void OnDisconnect() = 0;
  };

  // Does not take ownership of |delegate|, which must not be NULL.
  Login(Delegate* delegate,
        const buzz::XmppClientSettings& user_settings,
        const scoped_refptr<net::URLRequestContextGetter>&
            request_context_getter,
        const ServerList& servers,
        bool try_ssltcp_first,
        const std::string& auth_mechanism);
  virtual ~Login();

  void StartConnection();
  // The updated settings only take effect the next time StartConnection
  // is called.
  void UpdateXmppSettings(const buzz::XmppClientSettings& user_settings);

  // net::NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE;

  // SingleLoginAttempt::Delegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task) OVERRIDE;
  virtual void OnNeedReconnect() OVERRIDE;
  virtual void OnRedirect(const ServerInformation& redirect_server) OVERRIDE;

 private:
  void OnLogoff();

  // Stops any existing reconnect timer and sets an initial reconnect
  // interval.
  void ResetReconnectState();

  // Tries to reconnect in some point in the future.  If called
  // repeatedly, will wait longer and longer until reconnecting.
  void TryReconnect();

  // The actual function (called by |reconnect_timer_|) that does the
  // reconnection.
  void DoReconnect();

  Delegate* const delegate_;
  LoginSettings login_settings_;
  scoped_ptr<SingleLoginAttempt> single_attempt_;

  // reconnection state.
  base::TimeDelta reconnect_interval_;
  base::OneShotTimer<Login> reconnect_timer_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

// Workaround for MSVS 2005 bug that fails to handle inheritance from a nested
// class properly if it comes directly on a base class list.
typedef Login::Delegate LoginDelegate;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
