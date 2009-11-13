// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_FAILURE_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_FAILURE_H_

#include "talk/base/common.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class CaptchaChallenge;
}

namespace notifier {

class LoginFailure {
 public:
  enum LoginError {
    // Check the xmpp_error for more information.
    XMPP_ERROR,

    // If the certificate has expired, it usually means that the computer's
    // clock isn't set correctly.
    CERTIFICATE_EXPIRED_ERROR,

    // Apparently, there is a proxy that needs authentication information.
    PROXY_AUTHENTICATION_ERROR,
  };

  explicit LoginFailure(LoginError error);
  LoginFailure(LoginError error,
               buzz::XmppEngine::Error xmpp_error,
               int subcode);
  LoginFailure(LoginError error,
               buzz::XmppEngine::Error xmpp_error,
               int subcode,
               const buzz::CaptchaChallenge& captcha);

  // Used as the first level of error information.
  LoginError error() const {
    return error_;
  }

  // Returns the XmppEngine only. Valid if and only if error() == XMPP_ERROR.
  //
  // Handler should mimic logic from PhoneWindow::ShowConnectionError
  // (except that the DiagnoseConnectionError has already been done).
  buzz::XmppEngine::Error xmpp_error() const;

  // Returns the captcha challenge.  Valid if and only if
  //   xmpp_error is buzz::XmppEngine::ERROR_UNAUTHORIZED or
  //                 buzz::XmppEngine::ERROR_MISSING_USERNAME
  //
  // See PhoneWindow::HandleConnectionPasswordError for how to handle this
  // (after the if (..) { LoginAccountAndConnectionSetting(); ...} because
  // that is done by SingleLoginAttempt.
  const buzz::CaptchaChallenge& captcha() const;

 private:
  LoginError error_;
  buzz::XmppEngine::Error xmpp_error_;
  int subcode_;
  scoped_ptr<buzz::CaptchaChallenge> captcha_;

  DISALLOW_COPY_AND_ASSIGN(LoginFailure);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_LOGIN_FAILURE_H_
