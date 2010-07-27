// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LoginConnectionState is an enum representing the state of the XMPP
// connection.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_

namespace notifier {

enum LoginConnectionState {
  STATE_DISCONNECTED,
  STATE_CONNECTED,
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_CONNECTION_STATE_H_
