// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "jingle/notifier/base/xmpp_connection.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {

class ConnectionSettings;
class LoginSettings;

// Handles all of the aspects of a single login attempt (across multiple ip
// addresses) and maintainence. By containing this within one class, when
// another login attempt is made, this class will be disposed and all of the
// signalling for the previous login attempt will be cleaned up immediately.
class SingleLoginAttempt : public XmppConnection::Delegate,
                           public XmppConnectionGenerator::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnConnect(
        base::WeakPtr<buzz::XmppTaskParentInterface> base_task) = 0;
    virtual void OnNeedReconnect() = 0;
    virtual void OnRedirect(const std::string& redirect_server,
                            int redirect_port) = 0;
  };

  // Does not take ownership of |login_settings| or |delegate|.
  // Neither may be NULL.
  SingleLoginAttempt(LoginSettings* login_settings, Delegate* delegate);

  virtual ~SingleLoginAttempt();

  // XmppConnection::Delegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> parent) OVERRIDE;
  virtual void OnError(buzz::XmppEngine::Error error,
                       int error_subcode,
                       const buzz::XmlElement* stream_error) OVERRIDE;

  // XmppConnectionGenerator::Delegate implementation.
  virtual void OnNewSettings(const ConnectionSettings& new_settings) OVERRIDE;
  virtual void OnExhaustedSettings(bool successfully_resolved_dns,
                                   int first_dns_error) OVERRIDE;

 private:
  LoginSettings* login_settings_;
  Delegate* delegate_;
  XmppConnectionGenerator connection_generator_;
  scoped_ptr<XmppConnection> xmpp_connection_;

  DISALLOW_COPY_AND_ASSIGN(SingleLoginAttempt);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
