// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_SCREEN_CONNECTION_H_
#define API_PUBLIC_SCREEN_CONNECTION_H_

#include "base/macros.h"

namespace openscreen {

class ScreenConnection {
 public:
  ScreenConnection() = default;
  virtual ~ScreenConnection() = default;

  // TODO(mfoltz): Define extension API exposed to embedders.  This would be
  // used, for example, to query for and implement vendor-specific protocols
  // alongside the Open Screen Protocol.

  // NOTE: ScreenConnection instances that are owned by clients will have a
  // ScreenInfo attached with data from discovery and QUIC connection
  // establishment.  What about server connections?  We probably want to have
  // two different structures representing what the client and server know about
  // a connection.
};

class ScreenConnectionObserverBase {
 public:
  // Called when the state becomes kRunning.
  virtual void OnStarted() = 0;
  // Called when the state becomes kStopped.
  virtual void OnStopped() = 0;

  // Notifications to changes to connections.
  virtual void OnConnectionAdded(const ScreenConnection&) = 0;
  virtual void OnConnectionChanged(const ScreenConnection&) = 0;
  virtual void OnConnectionRemoved(const ScreenConnection&) = 0;

 protected:
  virtual ~ScreenConnectionObserverBase() = default;
};

}  // namespace openscreen

#endif  // API_PUBLIC_SCREEN_CONNECTION_H_
