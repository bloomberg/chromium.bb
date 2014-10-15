// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_OBSERVER_H
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_OBSERVER_H

#include <string>

#include "base/memory/scoped_ptr.h"

namespace proximity_auth {

struct RemoteStatusUpdate;

// An interface for observing events that happen on a Client.
class ClientObserver {
 public:
  // Called when sending an "Easy Unlock used"  local event message completes.
  // |success| is true iff the event was sent successfully.
  virtual void OnUnlockEventSent(bool success) = 0;

  // Called when a RemoteStatusUpdate is received.
  virtual void OnRemoteStatusUpdate(
      const RemoteStatusUpdate& status_update) = 0;

  // Called when a response to a 'decrypt_request' is received, with the
  // |decrypted_bytes| that were returned by the remote device. A null pointer
  // indicates failure.
  virtual void OnDecryptResponse(scoped_ptr<std::string> decrypted_bytes) = 0;

  // Called when a response to a 'unlock_request' is received.
  // |success| is true iff the request was made successfully.
  virtual void OnUnlockResponse(bool success) = 0;

  // Called when the underlying secure channel disconnects.
  virtual void OnDisconnected() = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_OBSERVER_H
