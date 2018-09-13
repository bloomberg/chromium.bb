// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_H_

namespace openscreen {

// Represents an embedder's view of a connection between an Open Screen
// controller and a receiver.  Both the controller and receiver will have a
// ProtocolConnection object, although the information known about the other party
// may not be symmetrical.
//
// A ProtocolConnection supports multiple protocols defined by the Open Screen
// standard and can be extended by embedders with additional protocols.
class ProtocolConnection {
 public:
  ProtocolConnection() = default;
  virtual ~ProtocolConnection() = default;

  // TODO(mfoltz): Define extension API exposed to embedders.  This would be
  // used, for example, to query for and implement vendor-specific protocols
  // alongside the Open Screen Protocol.

  // NOTE: ProtocolConnection instances that are owned by clients will have a
  // ScreenInfo attached with data from discovery and QUIC connection
  // establishment.  What about server connections?  We probably want to have
  // two different structures representing what the client and server know about
  // a connection.
};

class ProtocolConnectionObserver {
 public:
  // Called when the state becomes kRunning.
  virtual void OnRunning() = 0;
  // Called when the state becomes kStopped.
  virtual void OnStopped() = 0;

  // Called when a new connection was created between 5-tuples.
  virtual void OnConnectionAdded(const ProtocolConnection& connection) = 0;
  // Called when the state of |connection| has changed.
  virtual void OnConnectionChanged(const ProtocolConnection& connection) = 0;
  // Called when |connection| is no longer available, either because the
  // underlying transport was terminated, the underying system resource was
  // closed, or data can no longer be exchanged.
  virtual void OnConnectionRemoved(const ProtocolConnection& connection) = 0;

 protected:
  virtual ~ProtocolConnectionObserver() = default;
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_H_
