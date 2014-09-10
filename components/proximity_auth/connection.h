// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CONNECTION_H
#define COMPONENTS_PROXIMITY_AUTH_CONNECTION_H

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

class ConnectionObserver;
class WireMessage;

// Base class representing a connection with a remote device, which is a
// persistent bidirectional channel for sending and receiving wire messages.
class Connection {
 public:
  enum Status {
    DISCONNECTED,
    IN_PROGRESS,
    CONNECTED,
  };

  // Constructs a connection to the given |remote_device|.
  explicit Connection(const RemoteDevice& remote_device);
  ~Connection();

  // Returns true iff the connection's status is CONNECTED.
  bool IsConnected() const;

  // Sends a message to the remote device.
  // |OnSendCompleted()| will be called for all observers upon completion with
  // either success or failure.
  void SendMessage(scoped_ptr<WireMessage> message);

  void AddObserver(ConnectionObserver* observer);
  void RemoveObserver(ConnectionObserver* observer);

  const RemoteDevice& remote_device() const { return remote_device_; }

  // Abstract methods that subclasses should implement:

  // Pauses or unpauses the handling of incoming messages.  Pausing allows the
  // user of the connection to add or remove observers without missing messages.
  virtual void SetPaused(bool paused) = 0;

  // Attempts to connect to the remote device if not already connected.
  virtual void Connect() = 0;

  // Disconnects from the remote device.
  virtual void Disconnect() = 0;

 protected:
  // Sets the connection's status to |status|. If this is different from the
  // previous status, notifies observers of the change in status.
  void SetStatus(Status status);

  Status status() const { return status_; }

  // Called after attempting to send bytes over the connection, whether the
  // message was successfully sent or not.
  void OnDidSendMessage(const WireMessage& message, bool success);

  // Called when bytes are read from the connection. There should not be a send
  // in progress when this function is called.
  void OnBytesReceived(const std::string& bytes);

  // Sends bytes over the connection. The implementing class should call
  // OnSendCompleted() once the send succeeds or fails. At most one send will be
  // in progress.
  virtual void SendMessageImpl(scoped_ptr<WireMessage> message) = 0;

  // Returns |true| iff the |received_bytes_| are long enough to contain a
  // complete wire message. Exposed for testing.
  virtual bool HasReceivedCompleteMessage();

  // Deserializes the |recieved_bytes_| and returns the resulting WireMessage,
  // or NULL if the message is malformed. Exposed for testing.
  virtual scoped_ptr<WireMessage> DeserializeWireMessage();

 private:
  // The remote device corresponding to this connection.
  const RemoteDevice remote_device_;

  // The current status of the connection.
  Status status_;

  // The registered observers of the connection.
  ObserverList<ConnectionObserver> observers_;

  // A temporary buffer storing bytes received before a received message can be
  // fully constructed.
  std::string received_bytes_;

  // Whether a message is currently in the process of being sent.
  bool is_sending_message_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CONNECTION_H
