// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_

#include <string>

#include "jingle/notifier/communicator/login.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class AsyncSocket;
class PreXmppAuth;
class XmppClient;
class XmppClientSettings;
class XmppClientSettings;
}

namespace talk_base {
class FirewallManager;
struct ProxyInfo;
class SignalThread;
class Task;
}

namespace notifier {

class ConnectionSettings;
class LoginFailure;
class LoginSettings;
struct ServerInformation;
class XmppConnectionGenerator;

// Handles all of the aspects of a single login attempt (across multiple ip
// addresses) and maintainence. By containing this within one class, when
// another login attempt is made, this class will be disposed and all of the
// signalling for the previous login attempt will be cleaned up immediately.
//
// This is a task to allow for cleaning this up when a signal is being fired.
// Technically, delete this during the firing of a signal could work but it is
// fragile.
class SingleLoginAttempt : public talk_base::Task, public sigslot::has_slots<> {
 public:
  SingleLoginAttempt(talk_base::TaskParent* parent,
                     LoginSettings* login_settings);
  ~SingleLoginAttempt();
  virtual int ProcessStart();
  void UseNextConnection();
  void UseCurrentConnection();

  buzz::XmppClient* xmpp_client() {
    return client_;
  }

  // Returns the proxy that is being used to connect (or the default proxy
  // information if all attempted connections failed).
  const talk_base::ProxyInfo& proxy() const;

  // Typically handled by creating a new SingleLoginAttempt and doing
  // StartConnection.
  sigslot::signal0<> SignalUnexpectedDisconnect;

  // Typically handled by storing the redirect for 5 seconds, and setting it
  // into LoginSettings, then creating a new SingleLoginAttempt, and doing
  // StartConnection.
  //
  // SignalRedirect(const std::string& redirect_server, int redirect_port);
  sigslot::signal2<const std::string&, int> SignalRedirect;

  sigslot::signal0<> SignalNeedAutoReconnect;

  // SignalClientStateChange(buzz::XmppEngine::State new_state);
  sigslot::signal1<buzz::XmppEngine::State> SignalClientStateChange;

  // See the LoginFailure for how to handle this.
  sigslot::signal1<const LoginFailure&> SignalLoginFailure;

  // Sent when there is a graceful log-off (state goes to closed with no
  // error).
  sigslot::signal0<> SignalLogoff;

  sigslot::repeater2<const char*, int> SignalLogInput;
  sigslot::repeater2<const char*, int> SignalLogOutput;

 protected:
  virtual void Stop();

 private:
  void DoLogin(const ConnectionSettings& connection_settings);
  buzz::AsyncSocket* CreateSocket(const buzz::XmppClientSettings& xcs);
  static buzz::PreXmppAuth* CreatePreXmppAuth(
      const buzz::XmppClientSettings& xcs);

  // Cleans up any xmpp client state to get ready for a new one.
  void ClearClient();

  void HandleConnectionError(
      buzz::XmppEngine::Error code,
      int subcode,
      const buzz::XmlElement* stream_error);
  void HandleConnectionPasswordError();

  void OnAuthenticationError();
  void OnCertificateExpired();
  void OnClientStateChange(buzz::XmppEngine::State state);
  void OnClientStateChangeClosed(buzz::XmppEngine::State previous_state);
  void OnAttemptedAllConnections(bool successfully_resolved_dns,
                                 int first_dns_error);

  buzz::XmppEngine::State state_;
  buzz::XmppEngine::Error code_;
  int subcode_;
  bool need_authentication_;
  bool certificate_expired_;
  LoginSettings* login_settings_;
  buzz::XmppClient* client_;
  scoped_ptr<XmppConnectionGenerator> connection_generator_;

  DISALLOW_COPY_AND_ASSIGN(SingleLoginAttempt);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
