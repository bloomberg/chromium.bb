// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/login_failure.h"

#include "talk/xmpp/prexmppauth.h"

namespace notifier {

LoginFailure::LoginFailure(LoginError error)
  : error_(error),
    xmpp_error_(buzz::XmppEngine::ERROR_NONE),
    subcode_(0) {
}

LoginFailure::LoginFailure(LoginError error,
                           buzz::XmppEngine::Error xmpp_error,
                           int subcode)
  : error_(error),
    xmpp_error_(xmpp_error),
    subcode_(subcode) {
}

LoginFailure::LoginFailure(LoginError error,
                           buzz::XmppEngine::Error xmpp_error,
                           int subcode,
                           const buzz::CaptchaChallenge& captcha)
  : error_(error),
    xmpp_error_(xmpp_error),
    subcode_(subcode),
    captcha_(new buzz::CaptchaChallenge(captcha)) {
}

buzz::XmppEngine::Error LoginFailure::xmpp_error() const {
  ASSERT(error_ == XMPP_ERROR);
  return xmpp_error_;
}

const buzz::CaptchaChallenge& LoginFailure::captcha() const {
  ASSERT(xmpp_error_ == buzz::XmppEngine::ERROR_UNAUTHORIZED ||
         xmpp_error_ == buzz::XmppEngine::ERROR_MISSING_USERNAME);
  return *captcha_.get();
}

}  // namespace notifier
