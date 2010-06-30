// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/login_failure.h"

#include "base/logging.h"

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

buzz::XmppEngine::Error LoginFailure::xmpp_error() const {
  DCHECK_EQ(error_, XMPP_ERROR);
  return xmpp_error_;
}

}  // namespace notifier
