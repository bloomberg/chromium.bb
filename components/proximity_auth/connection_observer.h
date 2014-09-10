// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CONNECTION_OBSERVER_H
#define COMPONENTS_PROXIMITY_AUTH_CONNECTION_OBSERVER_H

#include "components/proximity_auth/connection.h"

namespace proximity_auth {

class WireMessage;

// An interface for observing events that happen on a Connection.
class ConnectionObserver {
 public:
  // Called when the |connection|'s status changes from |old_status| to
  // |new_status|.
  virtual void OnConnectionStatusChanged(const Connection& connection,
                                         Connection::Status old_status,
                                         Connection::Status new_status) = 0;

  // Called when a |message| is received from a remote device over the
  // |connection|.
  virtual void OnMessageReceived(const Connection& connection,
                                 const WireMessage& message) = 0;

  // Called after a |message| is sent to the remote device over the
  // |connection|. |success| is |true| iff the message is sent successfully.
  virtual void OnSendCompleted(const Connection& connection,
                               const WireMessage& message,
                               bool success) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CONNECTION_OBSERVER_H
