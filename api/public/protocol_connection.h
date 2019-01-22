// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_H_

#include <cstddef>
#include <cstdint>

namespace openscreen {

class Error;
struct NetworkMetrics;

// Represents an embedder's view of a connection between an Open Screen
// controller and a receiver.  Both the controller and receiver will have a
// ProtocolConnection object, although the information known about the other
// party may not be symmetrical.
//
// A ProtocolConnection supports multiple protocols defined by the Open Screen
// standard and can be extended by embedders with additional protocols.
class ProtocolConnection {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the state of |connection| has changed.
    virtual void OnConnectionChanged(const ProtocolConnection& connection) = 0;

    // Called when |connection| is no longer available, either because the
    // underlying transport was terminated, the underying system resource was
    // closed, or data can no longer be exchanged.
    virtual void OnConnectionClosed(const ProtocolConnection& connection) = 0;
  };

  ProtocolConnection(uint64_t endpoint_id, uint64_t connection_id);
  virtual ~ProtocolConnection() = default;

  // TODO(mfoltz): Define extension API exposed to embedders.  This would be
  // used, for example, to query for and implement vendor-specific protocols
  // alongside the Open Screen Protocol.

  // NOTE: ProtocolConnection instances that are owned by clients will have a
  // ServiceInfo attached with data from discovery and QUIC connection
  // establishment.  What about server connections?  We probably want to have
  // two different structures representing what the client and server know about
  // a connection.

  void SetObserver(Observer* observer);

  // TODO(btolsch): This should be derived from the handshake auth identifier
  // when that is finalized and implemented.
  uint64_t endpoint_id() const { return endpoint_id_; }
  uint64_t connection_id() const { return connection_id_; }

  virtual void Write(const uint8_t* data, size_t data_size) = 0;
  virtual void CloseWriteEnd() = 0;

 protected:
  uint64_t endpoint_id_;
  uint64_t connection_id_;
  Observer* observer_ = nullptr;
};

class ProtocolConnectionServiceObserver {
 public:
  // Called when the state becomes kRunning.
  virtual void OnRunning() = 0;
  // Called when the state becomes kStopped.
  virtual void OnStopped() = 0;

  // Called when metrics have been collected by the service.
  virtual void OnMetrics(const NetworkMetrics& metrics) = 0;
  // Called when an error has occurred.
  virtual void OnError(const Error& error) = 0;

 protected:
  virtual ~ProtocolConnectionServiceObserver() = default;
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_H_
