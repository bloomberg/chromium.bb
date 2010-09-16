// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_

#include <string>

#include "base/scoped_ptr.h"
#include "jingle/notifier/base/xmpp_connection.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"

namespace talk_base {
class Task;
}

namespace notifier {

class ConnectionSettings;
class LoginSettings;
class XmppConnectionGenerator;
class XmppConnection;

// Handles all of the aspects of a single login attempt (across multiple ip
// addresses) and maintainence. By containing this within one class, when
// another login attempt is made, this class will be disposed and all of the
// signalling for the previous login attempt will be cleaned up immediately.
class SingleLoginAttempt : public XmppConnection::Delegate,
                           public sigslot::has_slots<> {
 public:
  explicit SingleLoginAttempt(LoginSettings* login_settings);

  virtual ~SingleLoginAttempt();

  virtual void OnConnect(base::WeakPtr<talk_base::Task> parent);

  virtual void OnError(buzz::XmppEngine::Error error,
                       int error_subcode,
                       const buzz::XmlElement* stream_error);

  // Typically handled by storing the redirect for 5 seconds, and setting it
  // into LoginSettings, then creating a new SingleLoginAttempt, and doing
  // StartConnection.
  //
  // SignalRedirect(const std::string& redirect_server, int redirect_port);
  sigslot::signal2<const std::string&, int> SignalRedirect;

  sigslot::signal0<> SignalNeedAutoReconnect;

  sigslot::signal1<base::WeakPtr<talk_base::Task> > SignalConnect;

 private:
  void DoLogin(const ConnectionSettings& connection_settings);

  void OnAttemptedAllConnections(bool successfully_resolved_dns,
                                 int first_dns_error);

  LoginSettings* login_settings_;
  XmppConnectionGenerator connection_generator_;
  scoped_ptr<XmppConnection> xmpp_connection_;

  DISALLOW_COPY_AND_ASSIGN(SingleLoginAttempt);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
