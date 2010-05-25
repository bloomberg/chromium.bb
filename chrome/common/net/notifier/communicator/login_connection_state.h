// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LoginConnectionState is an enum representing the state of the XMPP
// connection.

#ifndef CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_
#define CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_

namespace notifier {

enum LoginConnectionState {
  STATE_CLOSED,
  // Same as the closed state but indicates that a countdown is
  // happening for auto-retrying the connection.
  STATE_RETRYING,
  STATE_OPENING,
  STATE_OPENED,
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_
