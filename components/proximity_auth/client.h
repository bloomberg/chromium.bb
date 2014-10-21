// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_H
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_H

#include <deque>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/proximity_auth/connection_observer.h"

namespace base {
class DictionaryValue;
}

namespace proximity_auth {

class ClientObserver;
class Connection;
class SecureContext;

// A client handling the Easy Unlock protocol, capable of parsing events from
// the remote device and sending events for the local device.
class Client : public ConnectionObserver {
 public:
  // Constructs a client that sends and receives messages over the given
  // |connection|, using the |secure_context| to encrypt and decrypt the
  // messages. The |connection| must be connected. The client begins observing
  // messages as soon as it is constructed.
  Client(scoped_ptr<Connection> connection,
         scoped_ptr<SecureContext> secure_context);
  virtual ~Client();

  // Adds or removes an observer for Client events.
  void AddObserver(ClientObserver* observer);
  void RemoveObserver(ClientObserver* observer);

  // Returns true iff the remote device supports the v3.1 sign-in protocol.
  bool SupportsSignIn() const;

  // Sends an unlock event message to the remote device.
  void DispatchUnlockEvent();

  // Sends a serialized SecureMessage to the remote device to decrypt the
  // |challenge|. OnDecryptResponse will be called for each observer when the
  // decrypted response is received.
  // TODO(isherman): Add params for the RSA private key and crypto delegate.
  void RequestDecryption(const std::string& challenge);

  // Sends a simple request to the remote device to unlock the screen.
  // OnUnlockResponse is called for each observer when the response is returned.
  void RequestUnlock();

 protected:
  // Exposed for testing.
  Connection* connection() { return connection_.get(); }
  SecureContext* secure_context() { return secure_context_.get(); }

 private:
  // Internal data structure to represent a pending message that either hasn't
  // been sent yet or is waiting for a response from the remote device.
  struct PendingMessage {
    PendingMessage();
    PendingMessage(const base::DictionaryValue& message);
    ~PendingMessage();

    // The message, serialized as JSON.
    const std::string json_message;

    // The message type. This is possible to parse from the |json_message|; it's
    // stored redundantly for convenience.
    const std::string type;
  };

  // Pops the first of the |queued_messages_| and sends it to the remote device.
  void ProcessMessageQueue();

  // Handles an incoming "status_update" |message|, parsing and notifying
  // observers of the content.
  void HandleRemoteStatusUpdateMessage(const base::DictionaryValue& message);

  // Handles an incoming "decrypt_response" message, parsing and notifying
  // observers of the decrypted content.
  void HandleDecryptResponseMessage(const base::DictionaryValue& message);

  // Handles an incoming "unlock_response" message, notifying observers of the
  // response.
  void HandleUnlockResponseMessage(const base::DictionaryValue& message);

  // ConnectionObserver:
  void OnConnectionStatusChanged(const Connection& connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& wire_message) override;
  void OnSendCompleted(const Connection& connection,
                       const WireMessage& wire_message,
                       bool success) override;

  // The connection used to send and receive events and status updates.
  scoped_ptr<Connection> connection_;

  // Used to encrypt and decrypt payloads sent and received over the
  // |connection_|.
  scoped_ptr<SecureContext> secure_context_;

  // The registered observers of |this_| client.
  ObserverList<ClientObserver> observers_;

  // Queue of messages to send to the remote device.
  std::deque<PendingMessage> queued_messages_;

  // The current message being sent or waiting on the remote device for a
  // response. Null if there is no message currently in this state.
  scoped_ptr<PendingMessage> pending_message_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_H
